// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_elf/third_party_dlls/hook.h"

#include <windows.h>

#include <assert.h>
#include <ntstatus.h>
#include <psapi.h>
#include <winternl.h>

#include <atomic>
#include <limits>
#include <string>

#include "base/compiler_specific.h"
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
#include "sandbox/win/src/service_resolver.h"

// http://blogs.msdn.com/oldnewthing/archive/2004/10/25/247180.aspx
extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace third_party_dlls {

namespace {

typedef ULONG SECTION_INHERIT;

typedef NTSTATUS(WINAPI* NtMapViewOfSectionFunction)(
    IN HANDLE SectionHandle,
    IN HANDLE ProcessHandle,
    IN OUT PVOID* BaseAddress,
    IN ULONG_PTR ZeroBits,
    IN SIZE_T CommitSize,
    IN OUT PLARGE_INTEGER SectionOffset OPTIONAL,
    IN OUT PSIZE_T ViewSize,
    IN SECTION_INHERIT InheritDisposition,
    IN ULONG AllocationType,
    IN ULONG Win32Protect);

// Allocate storage for the hook thunk in a page of this module to save on doing
// an extra allocation at run time. We chose a reasonable size value (128) and
// check that it's sufficient later when performing the hooks.
#pragma section(".crthunk", read, execute)
static __declspec(allocate(".crthunk")) BYTE g_thunk_storage[128];

// Set if and when ApplyHook() has been successfully executed.
bool g_hook_active = false;

// Indicates if the hook was disabled after the hook was activated.
std::atomic<bool> g_hook_disabled(false);

// Set to a different NTSTATUS if an error occured while applying the hook.
std::atomic<int32_t> g_apply_hook_result(STATUS_SUCCESS);

// Check if a given |process| handle references THIS process.
bool IsTargetCurrentProcess(HANDLE process) {
  return process == ::GetCurrentProcess() ||
         ::GetProcessId(process) == ::GetCurrentProcessId();
}

// Check if a given memory |section_base| is marked as an image.
bool IsSectionImage(PVOID section_base) {
  assert(section_base);
  MEMORY_BASIC_INFORMATION info = {};
  if (!::VirtualQuery(section_base, &info, sizeof(info))) {
    return false;
  }

  return (info.Type & MEM_IMAGE) == MEM_IMAGE;
}

// Query the full path backing the section.
// - Returned system wstring (UTF-16) will be empty() if anything fails.
std::wstring GetSectionName(PVOID section_base) {
  assert(section_base);

  constexpr DWORD kMaxNameSize = MAX_PATH + 1;
  WCHAR name[kMaxNameSize];

  DWORD size = ::GetMappedFileName(::GetCurrentProcess(), section_base, name,
                                   kMaxNameSize);
  if (size == 0 || size >= kMaxNameSize) {
    return std::wstring();
  }
  return name;
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
  const auto tolower = [](auto c) {
    return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
  };
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
    temp_section_basename[i] = tolower(temp_section_basename[i]);

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
// 5) Temporarily check old (deprecated) blocklist.
// 6) Unmap view if blocking required.
// 7) Log the result either way.
//------------------------------------------------------------------------------
// CFG must be disabled for the call to orig_MapViewOfSection below.
DISABLE_CFI_ICALL
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
      !IsSectionImage(*base)) {
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

  // Check sources for blocklist decision.
  bool block = false;

  if (!image_name.empty() &&
      IsModuleListed(image_name_hash, fingerprint_hash)) {
    // 1) Third-party DLL blocklist, check for image name from PE header.
    block = true;
  } else if (!section_basename.empty() &&
             section_basename_hash != image_name_hash &&
             IsModuleListed(section_basename_hash, fingerprint_hash)) {
    // 2) Third-party DLL blocklist, check for image name from the section.
    block = true;
  } else if (!image_name.empty() && DllMatch(image_name)) {
    // 3) Hard-coded blocklist with name from PE header (deprecated).
    block = true;
  } else if (!section_basename.empty() &&
             section_basename.compare(image_name) != 0 &&
             DllMatch(section_basename)) {
    // 4) Hard-coded blocklist with name from the section (deprecated).
    block = true;
  }
  // Else, no block.

  // UNMAP the view.  This image is being blocked.
  if (block) {
    ::UnmapViewOfFile(*base);
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
NTSTATUS WINAPI
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
  return NewNtMapViewOfSection(
      reinterpret_cast<NtMapViewOfSectionFunction>(g_thunk_storage), section,
      process, base, zero_bits, commit_size, offset, view_size, inherit,
      allocation_type, protect);
}
#endif

ThirdPartyStatus ApplyHook() {
  constexpr wchar_t kNtdllName[] = L"ntdll.dll";

  // Debug check: ApplyHook() should not be called more than once.
  assert(!g_hook_active);

  // Prep system-service thunk via the appropriate ServiceResolver instance.
  sandbox::ServiceResolverThunk thunk(::GetCurrentProcess(), /*relaxed=*/false);
  assert(sizeof(g_thunk_storage) >= thunk.GetThunkSize());

  // Set target process to self.
  thunk.AllowLocalPatches();

  // Mark the thunk storage as readable and writeable, since we
  // are ready to write to it now.
  DWORD old_protect = 0;
  if (!::VirtualProtect(g_thunk_storage, sizeof(g_thunk_storage),
                        PAGE_EXECUTE_READWRITE, &old_protect)) {
    return ThirdPartyStatus::kHookVirtualProtectFailure;
  }

  // Replace the default NtMapViewOfSection system service with our patched
  // version.
#if defined(_WIN64)
  void* entry_point = reinterpret_cast<void*>(&NewNtMapViewOfSection64);
#else
  void* entry_point = reinterpret_cast<void*>(&NewNtMapViewOfSection);
#endif  // defined(_WIN64)

  // Setup() applies the system-service patch, and stores a copy of the original
  // system service coded in |g_thunk_storage|.
  NTSTATUS ntstatus = thunk.Setup(
      ::GetModuleHandle(kNtdllName), reinterpret_cast<void*>(&__ImageBase),
      "NtMapViewOfSection", nullptr, entry_point, g_thunk_storage,
      sizeof(g_thunk_storage), nullptr);

  if (!NT_SUCCESS(ntstatus)) {
    // Remember the status code.
    g_apply_hook_result.store(ntstatus, std::memory_order_relaxed);

    return ThirdPartyStatus::kHookApplyFailure;
  }

  // Mark the thunk storage (original system service) as executable and prevent
  // any future writes to it.
  // - Do not treat this as a fatal error on failure.
  if (!::VirtualProtect(g_thunk_storage, sizeof(g_thunk_storage),
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
