// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/hook.h"

#include <atomic>
#include <limits>
#include <memory>

#include <assert.h>

#include "chrome/chrome_elf/crash/crash_helper.h"
#include "chrome/chrome_elf/hook_util/hook_util.h"
#include "chrome/chrome_elf/pe_image_safe/pe_image_safe.h"
#include "chrome/chrome_elf/sha1/sha1.h"
#include "chrome/chrome_elf/third_party_dlls/hardcoded_blocklist.h"
#include "chrome/chrome_elf/third_party_dlls/logs.h"
#include "chrome/chrome_elf/third_party_dlls/main.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_file.h"
#include "chrome/chrome_elf/third_party_dlls/packed_list_format.h"
#include "chrome/chrome_elf/third_party_dlls/public_api.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/nt_internals.h"
#include "sandbox/win/src/service_resolver.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace third_party_dlls {

// Allocate storage for the hook thunk in a page of this module to save on doing
// an extra allocation at run time.
#pragma section(".crthunk", read, execute)
__declspec(allocate(".crthunk")) sandbox::ThunkData g_thunk_storage;

#if defined(_WIN64)
// Allocate storage for the pointer to the old NtMapViewOfSectionFunction.
#pragma section(".oldntmap", write, read)
__declspec(allocate(".oldntmap"))
    NtMapViewOfSectionFunction g_nt_map_view_of_section_func = nullptr;
#endif

namespace {

NtQuerySectionFunction g_nt_query_section_func = nullptr;
NtQueryVirtualMemoryFunction g_nt_query_virtual_memory_func = nullptr;
NtUnmapViewOfSectionFunction g_nt_unmap_view_of_section_func = nullptr;

// Set if and when ApplyHook() has been successfully executed.
bool g_hook_active = false;

// Indicates if the hook was disabled after the hook was activated.
std::atomic<bool> g_hook_disabled(false);

// Set to a different NTSTATUS if an error occured while applying the hook.
std::atomic<int32_t> g_apply_hook_result(STATUS_SUCCESS);

// Set up NTDLL function pointers.
bool InitImports() {
  HMODULE ntdll = ::GetModuleHandleW(sandbox::kNtdllName);

  g_nt_query_section_func = reinterpret_cast<NtQuerySectionFunction>(
      ::GetProcAddress(ntdll, "NtQuerySection"));
  g_nt_query_virtual_memory_func =
      reinterpret_cast<NtQueryVirtualMemoryFunction>(
          ::GetProcAddress(ntdll, "NtQueryVirtualMemory"));
  g_nt_unmap_view_of_section_func =
      reinterpret_cast<NtUnmapViewOfSectionFunction>(
          ::GetProcAddress(ntdll, "NtUnmapViewOfSection"));

  return g_nt_query_section_func && g_nt_query_virtual_memory_func &&
         g_nt_unmap_view_of_section_func;
}

// Check if a given |process| handle references THIS process.
bool IsTargetCurrentProcess(HANDLE process) {
  return process == NtCurrentProcess ||
         ::GetProcessId(process) == ::GetCurrentProcessId();
}

// Check if a given memory |section| is marked as an image.
bool IsSectionImage(HANDLE section) {
  assert(section && section != INVALID_HANDLE_VALUE);

  SECTION_BASIC_INFORMATION basic_info;
  SIZE_T bytes_returned;
  NTSTATUS ret =
      g_nt_query_section_func(section, SectionBasicInformation, &basic_info,
                              sizeof(basic_info), &bytes_returned);

  if (!NT_SUCCESS(ret) || sizeof(basic_info) != bytes_returned)
    return false;

  return !!(basic_info.Attributes & SEC_IMAGE);
}

// Query the full path backing the section.
// - Returned system wstring (UTF-16) will be empty() if anything fails.
std::wstring GetSectionName(PVOID section_base) {
  assert(section_base);
  assert(g_nt_query_virtual_memory_func);

  // NtQueryVirtualMemory MemorySectionName returns a UNICODE_STRING.
  std::vector<BYTE> buffer_data(MAX_PATH * sizeof(wchar_t));

  for (;;) {
    MEMORY_SECTION_NAME* memory_section_name =
        reinterpret_cast<MEMORY_SECTION_NAME*>(buffer_data.data());

    SIZE_T returned_bytes;
    NTSTATUS ret = g_nt_query_virtual_memory_func(
        NtCurrentProcess, section_base, MemorySectionName, memory_section_name,
        buffer_data.size(), &returned_bytes);

    if (STATUS_BUFFER_OVERFLOW == ret) {
      // Retry the call with the given buffer size.
      buffer_data.resize(returned_bytes);
      continue;
    }
    if (!NT_SUCCESS(ret))
      break;

    // Got it.
    // Note: UNICODE_STRING::Length is number of bytes, with no terminating null
    // char.
    UNICODE_STRING* unicode_string =
        reinterpret_cast<UNICODE_STRING*>(memory_section_name);

    return std::wstring(unicode_string->Buffer, 0,
                        unicode_string->Length / sizeof(wchar_t));
  }

  return std::wstring();
}

// Utility function for converting UTF-16 to UTF-8.
bool UTF16ToUTF8(const std::wstring& utf16, std::string* utf8) {
  assert(utf8);

  if (utf16.empty()) {
    utf8->clear();
    return true;
  }

  int size_needed_bytes = ::WideCharToMultiByte(CP_UTF8, 0, &utf16[0],
                                                static_cast<int>(utf16.size()),
                                                nullptr, 0, nullptr, nullptr);
  if (!size_needed_bytes)
    return false;

  utf8->resize(size_needed_bytes);
  return ::WideCharToMultiByte(CP_UTF8, 0, &utf16[0],
                               static_cast<int>(utf16.size()), &(*utf8)[0],
                               size_needed_bytes, nullptr, nullptr);
}

// Helper function to contain the data mining for the values needed.
// - |image_name| and |section_basename| will be lowercased.  |section_path|
//   will be left untouched, preserved for case-sensitive operations with it.
// - All strings returned are UTF-8.  Treat accordingly.
// - This function succeeds if image_name || section_* is found.
// Note: |section_path| contains |section_basename|, if the section name is
//       successfully mined.
bool GetDataFromImage(PVOID buffer,
                      DWORD buffer_size,
                      DWORD* time_date_stamp,
                      DWORD* image_size,
                      std::string* image_name,
                      std::string* section_path,
                      std::string* section_basename) {
  assert(buffer && buffer_size && time_date_stamp && image_size && image_name &&
         section_path && section_basename);

  image_name->clear();
  section_path->clear();
  section_basename->clear();

  pe_image_safe::PEImageSafe image(reinterpret_cast<HMODULE>(buffer),
                                   buffer_size);
  PIMAGE_FILE_HEADER file_header = image.GetFileHeader();
  if (!file_header ||
      image.GetImageBitness() == pe_image_safe::ImageBitness::kUnknown) {
    return false;
  }

  *time_date_stamp = file_header->TimeDateStamp;
  if (image.GetImageBitness() == pe_image_safe::ImageBitness::k64) {
    PIMAGE_OPTIONAL_HEADER64 opt_header =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER64>(image.GetOptionalHeader());
    *image_size = opt_header->SizeOfImage;
  } else {
    PIMAGE_OPTIONAL_HEADER32 opt_header =
        reinterpret_cast<PIMAGE_OPTIONAL_HEADER32>(image.GetOptionalHeader());
    *image_size = opt_header->SizeOfImage;
  }

  //----------------------------------------------------------------------------
  // Get the module name 1) out of PE Export Dir, and 2) from the memory section
  // name.
  PIMAGE_EXPORT_DIRECTORY exports = image.GetExportDirectory();
  // IMAGE_EXPORT_DIRECTORY::Name is a RVA to an asciiz string with the name of
  // this DLL.  Ascii is UTF-8 compliant.
  if (exports && exports->Name + MAX_PATH <= buffer_size) {
    const char* name =
        reinterpret_cast<const char*>(image.RVAToAddr(exports->Name));
    *image_name = std::string(name, ::strnlen(name, MAX_PATH));
  }

  // Lowercase |image_name|.
  for (size_t i = 0; i < image_name->size(); i++)
    (*image_name)[i] = tolower((*image_name)[i]);

  std::wstring temp_section_path = GetSectionName(buffer);

  // For now, consider it a success if at least one source results in a name.
  // Allow for the rare case of one or the other not being there.
  // (E.g.: a module could have no export directory.)
  if (image_name->empty() && temp_section_path.empty())
    return false;

  // There is further processing to do on the section path now.
  if (temp_section_path.empty())
    return true;

  // Extract the section basename from the section path.
  std::wstring temp_section_basename;
  size_t sep = temp_section_path.find_last_of('\\');
  if (sep == std::string::npos && !temp_section_path.empty()) {
    temp_section_basename = temp_section_path;
  } else if (sep != std::string::npos && temp_section_path.back() != '\\') {
    temp_section_basename = temp_section_path.substr(sep + 1);
  }

  // Lowercase |section_basename|.
  for (size_t i = 0; i < temp_section_basename.size(); i++)
    temp_section_basename[i] = towlower(temp_section_basename[i]);

  // Convert section strings from UTF-16 to UTF-8.
  return UTF16ToUTF8(temp_section_path, section_path) &&
         UTF16ToUTF8(temp_section_basename, section_basename);
}

//------------------------------------------------------------------------------
// Local interceptor function implementation
//
// 1) Allow mapping.
// 2) Return if OS failure or not interested in section.
// 3) Mine the data needed out of the PE headers.
// 4) Lookup module in local cache (blocking).
// 5) Temporarily check old (deprecated) blacklist.
// 6) Unmap view if blocking required.
// 7) Log the result either way.
//------------------------------------------------------------------------------

NTSTATUS NewNtMapViewOfSectionImpl(
    NtMapViewOfSectionFunction orig_MapViewOfSection,
    HANDLE section,
    HANDLE process,
    PVOID* base,
    ULONG_PTR zero_bits,
    SIZE_T commit_size,
    PLARGE_INTEGER offset,
    PSIZE_T view_size,
    SECTION_INHERIT inherit,
    ULONG allocation_type,
    ULONG protect) {
  assert(IsThirdPartyInitialized());

  // Need to initially allow the mapping to go through, to access the module
  // info we really need to make any decisions.  It will be unmapped if
  // necessary later in this function.
  NTSTATUS ret = orig_MapViewOfSection(section, process, base, zero_bits,
                                       commit_size, offset, view_size, inherit,
                                       allocation_type, protect);

  if (g_hook_disabled.load(std::memory_order_relaxed))
    return ret;

  // If there was an OS-level failure, if the mapping target is NOT this
  // process, or if the section is not a (valid) Portable Executable,
  // we're not interested.  Return the OS-level result code.
  if (!NT_SUCCESS(ret) || !IsTargetCurrentProcess(process) ||
      !IsSectionImage(section)) {
    return ret;
  }

  // Mine the data needed from the PE headers.
  DWORD time_date_stamp = 0;
  DWORD image_size = 0;
  std::string image_name;
  std::string section_path;
  std::string section_basename;

  assert(*view_size < std::numeric_limits<DWORD>::max());
  // A memory section can be > 32-bits, but an image/PE in memory can only be <=
  // 32-bits in size.  That's a limitation of Windows and its interactions with
  // processors.  No section that appears to be an image (checked above) should
  // have such a large size.
  if (!GetDataFromImage(*base, static_cast<DWORD>(*view_size), &time_date_stamp,
                        &image_size, &image_name, &section_path,
                        &section_basename)) {
    return ret;
  }

  // Note that one of either image_name or section_basename can be empty.
  elf_sha1::Digest image_name_hash;
  if (!image_name.empty())
    image_name_hash = elf_sha1::SHA1HashString(image_name);
  elf_sha1::Digest section_basename_hash;
  if (!section_basename.empty())
    section_basename_hash = elf_sha1::SHA1HashString(section_basename);
  elf_sha1::Digest fingerprint_hash = elf_sha1::SHA1HashString(
      GetFingerprintString(time_date_stamp, image_size));

  // Check sources for blacklist decision.
  bool block = false;

  if (!image_name.empty() &&
      IsModuleListed(image_name_hash, fingerprint_hash)) {
    // 1) Third-party DLL blacklist, check for image name from PE header.
    block = true;
  } else if (!section_basename.empty() &&
             section_basename_hash != image_name_hash &&
             IsModuleListed(section_basename_hash, fingerprint_hash)) {
    // 2) Third-party DLL blacklist, check for image name from the section.
    block = true;
  } else if (!image_name.empty() && DllMatch(image_name)) {
    // 3) Hard-coded blacklist with name from PE header (deprecated).
    block = true;
  } else if (!section_basename.empty() &&
             section_basename.compare(image_name) != 0 &&
             DllMatch(section_basename)) {
    // 4) Hard-coded blacklist with name from the section (deprecated).
    block = true;
  }
  // Else, no block.

  // UNMAP the view.  This image is being blocked.
  if (block) {
    assert(g_nt_unmap_view_of_section_func);
    g_nt_unmap_view_of_section_func(process, *base);
    *base = nullptr;
    ret = STATUS_UNSUCCESSFUL;
  }

  // LOG!
  // - If there was a failure getting |section_path|, at least pass image_name.
  LogLoadAttempt((block ? third_party_dlls::LogType::kBlocked
                        : third_party_dlls::LogType::kAllowed),
                 image_size, time_date_stamp,
                 section_path.empty() ? image_name : section_path);

  return ret;
}

}  // namespace

//------------------------------------------------------------------------------
// Public defines & functions
//------------------------------------------------------------------------------

// Interception of NtMapViewOfSection within the current process.
// - This/these replacement functions should never be called directly.  They are
//   called from the hook patch.
SANDBOX_INTERCEPT NTSTATUS WINAPI
NewNtMapViewOfSection(NtMapViewOfSectionFunction orig_MapViewOfSection,
                      HANDLE section,
                      HANDLE process,
                      PVOID* base,
                      ULONG_PTR zero_bits,
                      SIZE_T commit_size,
                      PLARGE_INTEGER offset,
                      PSIZE_T view_size,
                      SECTION_INHERIT inherit,
                      ULONG allocation_type,
                      ULONG protect) {
  NTSTATUS ret = STATUS_UNSUCCESSFUL;

  __try {
    ret = NewNtMapViewOfSectionImpl(
        orig_MapViewOfSection, section, process, base, zero_bits, commit_size,
        offset, view_size, inherit, allocation_type, protect);
  } __except (elf_crash::GenerateCrashDump(GetExceptionInformation())) {
  }

  return ret;
}

#if defined(_WIN64)
// x64 has an extra layer of indirection.  This just wraps the above
// interceptor function that x86 hits directly.
NTSTATUS WINAPI NewNtMapViewOfSection64(HANDLE section,
                                        HANDLE process,
                                        PVOID* base,
                                        ULONG_PTR zero_bits,
                                        SIZE_T commit_size,
                                        PLARGE_INTEGER offset,
                                        PSIZE_T view_size,
                                        SECTION_INHERIT inherit,
                                        ULONG allocation_type,
                                        ULONG protect) {
  return NewNtMapViewOfSection(g_nt_map_view_of_section_func, section, process,
                               base, zero_bits, commit_size, offset, view_size,
                               inherit, allocation_type, protect);
}
#endif

ThirdPartyStatus ApplyHook() {
  // Debug check: ApplyHook() should not be called more than once.
  assert(!g_hook_active);

  if (!InitImports())
    return ThirdPartyStatus::kHookInitImportsFailure;

  // Prep system-service thunk via the appropriate ServiceResolver instance.
  std::unique_ptr<sandbox::ServiceResolverThunk> thunk(
      elf_hook::HookSystemService(false));
  if (!thunk)
    return ThirdPartyStatus::kHookUnsupportedOs;

  // Set target process to self.
  thunk->AllowLocalPatches();

  // Mark the thunk storage as readable and writeable, since we
  // are ready to write to it now.
  DWORD old_protect = 0;
  if (!::VirtualProtect(&g_thunk_storage, sizeof(g_thunk_storage),
                        PAGE_EXECUTE_READWRITE, &old_protect)) {
    return ThirdPartyStatus::kHookVirtualProtectFailure;
  }

  // Replace the default NtMapViewOfSection system service with our patched
  // version.
  BYTE* thunk_storage = reinterpret_cast<BYTE*>(&g_thunk_storage);
  NTSTATUS ntstatus = STATUS_UNSUCCESSFUL;
#if defined(_WIN64)
  // Setup() applies the system-service patch, and stores a copy of the original
  // system service coded in |thunk_storage|.
  ntstatus =
      thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                   reinterpret_cast<void*>(&__ImageBase), "NtMapViewOfSection",
                   nullptr, reinterpret_cast<void*>(&NewNtMapViewOfSection64),
                   thunk_storage, sizeof(sandbox::ThunkData), nullptr);

  // Keep a pointer to the original system-service code, which is now in
  // |thunk_storage|.  Use this pointer for passing off execution from new shim.
  // - Only needed on x64.
  g_nt_map_view_of_section_func =
      reinterpret_cast<NtMapViewOfSectionFunction>(thunk_storage);

  // Ensure that the pointer to the old function can't be changed.
  // - Do not treat this as a fatal error on failure.
  if (!VirtualProtect(&g_nt_map_view_of_section_func,
                      sizeof(g_nt_map_view_of_section_func), PAGE_EXECUTE_READ,
                      &old_protect)) {
    assert(false);
  }
#else   // x86
  ntstatus =
      thunk->Setup(::GetModuleHandle(sandbox::kNtdllName),
                   reinterpret_cast<void*>(&__ImageBase), "NtMapViewOfSection",
                   nullptr, reinterpret_cast<void*>(&NewNtMapViewOfSection),
                   thunk_storage, sizeof(sandbox::ThunkData), nullptr);
#endif  // defined(_WIN64)

  if (!NT_SUCCESS(ntstatus)) {
    // Remember the status code.
    g_apply_hook_result.store(ntstatus, std::memory_order_relaxed);

    return ThirdPartyStatus::kHookApplyFailure;
  }

  // Mark the thunk storage (original system service) as executable and prevent
  // any future writes to it.
  // - Do not treat this as a fatal error on failure.
  if (!VirtualProtect(&g_thunk_storage, sizeof(g_thunk_storage),
                      PAGE_EXECUTE_READ, &old_protect)) {
    assert(false);
  }

  g_hook_active = true;

  return ThirdPartyStatus::kSuccess;
}

bool GetDataFromImageForTesting(PVOID mapped_image,
                                DWORD* time_date_stamp,
                                DWORD* image_size,
                                std::string* image_name,
                                std::string* section_path,
                                std::string* section_basename) {
  if (!g_nt_query_virtual_memory_func)
    InitImports();

  return GetDataFromImage(mapped_image, pe_image_safe::kImageSizeNotSet,
                          time_date_stamp, image_size, image_name, section_path,
                          section_basename);
}

}  // namespace third_party_dlls

void DisableHook() {
  third_party_dlls::g_hook_disabled.store(true, std::memory_order_relaxed);
}

int32_t GetApplyHookResult() {
  return third_party_dlls::g_apply_hook_result.load(std::memory_order_relaxed);
}
