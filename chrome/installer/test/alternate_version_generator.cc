// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

// The file contains the implementation of the mini_installer re-versioner.
// The main function (GenerateNextVersion) does the following in a temp dir:
// - Extracts and unpacks setup.exe and the Chrome-bin folder from
//   mini_installer.exe.
// - Inspects setup.exe to determine the current version.
// - Runs through all .dll and .exe files:
//   - Replacing all occurrences of the Unicode version string in the files'
//     resources with the updated string.
//   - For all resources in which the string substitution is made, the binary
//     form of the version is also replaced.
// - Re-packs setup.exe and Chrome-bin.
// - Inserts them into the target mini_installer.exe.
//
// This code assumes that the host program 1) initializes the process-wide
// CommandLine instance, and 2) resides in the output directory of a build
// tree.  When #2 is not the case, the --7za_path command-line switch may be
// used to provide the (relative or absolute) path to the directory containing
// 7za.exe.

#include "chrome/installer/test/alternate_version_generator.h"

#include <windows.h>

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/pe_image.h"
#include "base/win/scoped_handle.h"
#include "chrome/installer/test/pe_image_resources.h"
#include "chrome/installer/test/resource_loader.h"
#include "chrome/installer/test/resource_updater.h"
#include "chrome/installer/util/lzma_util.h"

namespace {

const wchar_t k7zaExe[] = L"7za.exe";
const wchar_t k7zaPathRelative[] = L"..\\..\\third_party\\lzma_sdk\\bin\\win64";
const wchar_t kB7[] = L"B7";
const wchar_t kBl[] = L"BL";
const wchar_t kBN[] = L"BN";
const wchar_t kChromeBin[] = L"Chrome-bin";
const wchar_t kChromePacked7z[] = L"CHROME.PACKED.7Z";
const wchar_t kChrome7z[] = L"CHROME.7Z";
const wchar_t kExe[] = L"exe";
const wchar_t kExpandExe[] = L"expand.exe";
const wchar_t kExtDll[] = L".dll";
const wchar_t kExtExe[] = L".exe";
const wchar_t kMakeCab[] = L"makecab.exe";
const wchar_t kSetupEx_[] = L"setup.ex_";
const wchar_t kSetupExe[] = L"setup.exe";
const char kSwitch7zaPath[] = "7za_path";
const wchar_t kTempDirPrefix[] = L"mini_installer_test_temp";

// A helper class for creating and cleaning a temporary directory.  A temporary
// directory is created in Initialize and destroyed (along with all of its
// contents) when the guard instance is destroyed.
class ScopedTempDirectory {
 public:
  ScopedTempDirectory() {}

  ScopedTempDirectory(const ScopedTempDirectory&) = delete;
  ScopedTempDirectory& operator=(const ScopedTempDirectory&) = delete;

  ~ScopedTempDirectory() {
    if (!directory_.empty() && !base::DeletePathRecursively(directory_)) {
      LOG(DFATAL) << "Failed deleting temporary directory \""
                  << directory_.value() << "\"";
    }
  }
  // Creates a temporary directory.
  bool Initialize() {
    DCHECK(directory_.empty());
    if (!base::CreateNewTempDirectory(&kTempDirPrefix[0], &directory_)) {
      LOG(DFATAL) << "Failed creating temporary directory.";
      return false;
    }
    return true;
  }
  const base::FilePath& directory() const {
    DCHECK(!directory_.empty());
    return directory_;
  }

 private:
  base::FilePath directory_;
};  // class ScopedTempDirectory

// A helper class for manipulating a Chrome product version.
class ChromeVersion {
 public:
  static ChromeVersion FromHighLow(DWORD high, DWORD low) {
    return ChromeVersion(static_cast<ULONGLONG>(high) << 32 |
                         static_cast<ULONGLONG>(low));
  }
  static ChromeVersion FromString(const std::string& version_string) {
    base::Version version(version_string);
    DCHECK(version.IsValid());
    const std::vector<uint32_t>& c(version.components());
    return ChromeVersion(static_cast<ULONGLONG>(c[0]) << 48 |
                         static_cast<ULONGLONG>(c[1]) << 32 |
                         static_cast<ULONGLONG>(c[2]) << 16 |
                         static_cast<ULONGLONG>(c[3]));
  }

  ChromeVersion() {}
  explicit ChromeVersion(ULONGLONG value) : version_(value) {}
  WORD major() const { return static_cast<WORD>(version_ >> 48); }
  WORD minor() const { return static_cast<WORD>(version_ >> 32); }
  WORD build() const { return static_cast<WORD>(version_ >> 16); }
  WORD patch() const { return static_cast<WORD>(version_); }
  DWORD high() const { return static_cast<DWORD>(version_ >> 32); }
  DWORD low() const { return static_cast<DWORD>(version_); }
  ULONGLONG value() const { return version_; }
  void set_value(ULONGLONG value) { version_ = value; }
  std::wstring ToString() const;
  std::string ToASCII() const;

 private:
  ULONGLONG version_;
};  // class ChromeVersion

std::wstring ChromeVersion::ToString() const {
  wchar_t buffer[24];
  int string_len = swprintf_s(&buffer[0], std::size(buffer), L"%hu.%hu.%hu.%hu",
                              major(), minor(), build(), patch());
  DCHECK_NE(-1, string_len);
  DCHECK_GT(static_cast<int>(std::size(buffer)), string_len);
  return std::wstring(&buffer[0], string_len);
}

std::string ChromeVersion::ToASCII() const {
  char buffer[24];
  int string_len = sprintf_s(&buffer[0], std::size(buffer), "%hu.%hu.%hu.%hu",
                             major(), minor(), build(), patch());
  DCHECK_NE(-1, string_len);
  DCHECK_GT(static_cast<int>(std::size(buffer)), string_len);
  return std::string(&buffer[0], string_len);
}

// Calls CreateProcess with good default parameters and waits for the process
// to terminate returning the process exit code.
bool RunProcessAndWait(const wchar_t* exe_path,
                       const std::wstring& cmdline,
                       int* exit_code) {
  bool result = true;
  base::LaunchOptions options;
  options.wait = true;
  options.start_hidden = true;
  base::Process process = base::LaunchProcess(cmdline, options);
  if (process.IsValid()) {
    if (exit_code) {
      if (!GetExitCodeProcess(process.Handle(),
                              reinterpret_cast<DWORD*>(exit_code))) {
        PLOG(DFATAL) << "Failed getting the exit code for \"" << cmdline
                     << "\".";
        result = false;
      } else {
        DCHECK_NE(*exit_code, static_cast<int>(STILL_ACTIVE));
      }
    }
  } else {
    result = false;
  }

  return result;
}

// Retrieves the version number of |pe_file| from its version
// resource, placing the value in |version|.  Returns true on success.
bool GetFileVersion(const base::FilePath& pe_file, ChromeVersion* version) {
  DCHECK(version);
  bool result = false;
  upgrade_test::ResourceLoader pe_file_loader;
  std::pair<const uint8_t*, DWORD> version_info_data;

  if (pe_file_loader.Initialize(pe_file) &&
      pe_file_loader.Load(
          VS_VERSION_INFO,
          static_cast<WORD>(reinterpret_cast<uintptr_t>(RT_VERSION)),
          &version_info_data)) {
    const VS_FIXEDFILEINFO* fixed_file_info;
    UINT ver_info_len;
    if (VerQueryValue(version_info_data.first, L"\\",
                      reinterpret_cast<void**>(
                          const_cast<VS_FIXEDFILEINFO**>(&fixed_file_info)),
                      &ver_info_len) != 0) {
      DCHECK_EQ(sizeof(VS_FIXEDFILEINFO), static_cast<size_t>(ver_info_len));
      *version = ChromeVersion::FromHighLow(fixed_file_info->dwFileVersionMS,
                                            fixed_file_info->dwFileVersionLS);
      result = true;
    } else {
      LOG(DFATAL) << "VerQueryValue failed to retrieve VS_FIXEDFILEINFO";
    }
  }

  return result;
}

// Retrieves the version number of setup.exe in |work_dir| from its version
// resource, placing the value in |version|.  Returns true on success.
bool GetSetupExeVersion(const base::FilePath& work_dir,
                        ChromeVersion* version) {
  return GetFileVersion(work_dir.Append(&kSetupExe[0]), version);
}

// Replace all occurrences in the sequence [|dest_first|, |dest_last|) that
// equals [|src_first|, |src_last|) with the sequence at |replacement_first| of
// the same length.  Returns true on success.  If non-nullptr,
// |replacements_made| is set to true/false accordingly.
bool ReplaceAll(uint8_t* dest_first,
                uint8_t* dest_last,
                const uint8_t* src_first,
                const uint8_t* src_last,
                const uint8_t* replacement_first,
                bool* replacements_made) {
  bool result = true;
  bool changed = false;
  do {
    dest_first = std::search(dest_first, dest_last, src_first, src_last);
    if (dest_first == dest_last) {
      break;
    }
    changed = true;
    if (memcpy_s(dest_first, dest_last - dest_first, replacement_first,
                 src_last - src_first) != 0) {
      result = false;
      break;
    }
    dest_first += (src_last - src_first);
  } while (true);

  if (replacements_made != nullptr) {
    *replacements_made = changed;
  }

  return result;
}

// A context structure in support of our EnumResource_Fn callback.
struct VisitResourceContext {
  ChromeVersion current_version;
  std::wstring current_version_str;
  ChromeVersion new_version;
  std::wstring new_version_str;
};  // struct VisitResourceContext

// Replaces the old version with the new in a resource.  A first pass is made to
// replace the string form (e.g., "9.0.584.0").  If any replacements are made, a
// second pass is made to replace the binary form (e.g., 0x0000024800000009).
// A final pass is made to replace the ASCII string form.
void VisitResource(const upgrade_test::EntryPath& path,
                   uint8_t* data,
                   DWORD size,
                   DWORD code_page,
                   uintptr_t context) {
  VisitResourceContext& ctx = *reinterpret_cast<VisitResourceContext*>(context);
  DCHECK_EQ(ctx.current_version_str.size(), ctx.new_version_str.size());

  // Replace all occurrences of current_version_str with new_version_str
  bool changing_version = false;
  if (ReplaceAll(
          data, data + size,
          reinterpret_cast<const uint8_t*>(ctx.current_version_str.c_str()),
          reinterpret_cast<const uint8_t*>(ctx.current_version_str.c_str() +
                                           ctx.current_version_str.size() + 1),
          reinterpret_cast<const uint8_t*>(ctx.new_version_str.c_str()),
          &changing_version) &&
      changing_version) {
    // Replace all binary occurrences of current_version with new_version.
    struct VersionPair {
      DWORD high;
      DWORD low;
    };
    VersionPair cur_ver = {ctx.current_version.high(),
                           ctx.current_version.low()};
    VersionPair new_ver = {ctx.new_version.high(), ctx.new_version.low()};
    ReplaceAll(data, data + size, reinterpret_cast<const uint8_t*>(&cur_ver),
               reinterpret_cast<const uint8_t*>(&cur_ver) + sizeof(cur_ver),
               reinterpret_cast<const uint8_t*>(&new_ver), nullptr);
  }

  // Replace all ASCII occurrences of current_version with new_version.
  std::string current_version(ctx.current_version.ToASCII());
  std::string new_version(ctx.new_version.ToASCII());
  ReplaceAll(
      data, data + size, reinterpret_cast<uint8_t*>(&current_version[0]),
      reinterpret_cast<uint8_t*>(&current_version[current_version.size()]),
      reinterpret_cast<uint8_t*>(&new_version[0]), nullptr);
}

// Updates version strings found in an image's .rdata (read-only data) section.
// This handles uses of CHROME_VERSION_STRING (e.g., chrome::kChromeVersion).
// Version numbers found even as substrings of larger strings are updated.
bool UpdateVersionInData(base::win::PEImage* image,
                         VisitResourceContext* context) {
  DCHECK_EQ(context->current_version_str.size(),
            context->new_version_str.size());
  IMAGE_SECTION_HEADER* rdata_header =
      image->GetImageSectionHeaderByName(".rdata");
  if (!rdata_header) {
    return true;  // Nothing to update.
  }

  size_t size = rdata_header->SizeOfRawData;
  uint8_t* data = reinterpret_cast<uint8_t*>(image->module()) +
                  rdata_header->PointerToRawData;

  // Replace all wide string occurrences of |current_version| with
  // |new_version|. No attempt is made to ensure that the modified bytes are
  // truly part of a wide string.
  const std::wstring& current_version_str = context->current_version_str;
  ReplaceAll(data, data + size,
             reinterpret_cast<const uint8_t*>(&current_version_str[0]),
             reinterpret_cast<const uint8_t*>(
                 &current_version_str[current_version_str.size()]),
             reinterpret_cast<const uint8_t*>(context->new_version_str.c_str()),
             nullptr);

  // Replace all ASCII occurrences of |current_version| with |new_version|. No
  // attempt is made to ensure that the modified bytes are truly part of an
  // ASCII string.
  std::string current_version(context->current_version.ToASCII());
  std::string new_version(context->new_version.ToASCII());
  ReplaceAll(
      data, data + size, reinterpret_cast<uint8_t*>(&current_version[0]),
      reinterpret_cast<uint8_t*>(&current_version[current_version.size()]),
      reinterpret_cast<uint8_t*>(&new_version[0]), nullptr);

  return true;
}

// Updates the version strings and numbers in all of |image_file|'s resources.
bool UpdateVersionIfMatch(const base::FilePath& image_file,
                          VisitResourceContext* context) {
  if (!context ||
      context->current_version_str.size() < context->new_version_str.size()) {
    return false;
  }

  uint32_t flags = base::File::FLAG_OPEN | base::File::FLAG_READ |
                   base::File::FLAG_WRITE |
                   base::File::FLAG_WIN_EXCLUSIVE_READ |
                   base::File::FLAG_WIN_EXCLUSIVE_WRITE;
  base::File file(image_file, flags);
  // It turns out that the underlying CreateFile can fail due to unhelpful
  // security software locking the newly created DLL. So add a few brief
  // retries to help tests that use this pass on machines thusly encumbered.
  int retries = 3;
  while (!file.IsValid() && retries-- > 0) {
    LOG(WARNING) << "Failed to open \"" << image_file.value() << "\"."
                 << " Retrying " << retries << " more times.";
    Sleep(1000);
    file.Initialize(image_file, flags);
  }

  if (!file.IsValid()) {
    PLOG(DFATAL) << "Failed to open \"" << image_file.value() << "\"";
    return false;
  }

  // Map the file into memory for modification (give the mapping its own handle
  // to the file so that its last-modified time can be updated below).
  base::MemoryMappedFile image_mapping;
  if (!image_mapping.Initialize(file.Duplicate(),
                                base::MemoryMappedFile::READ_WRITE)) {
    return false;
  }

  base::win::PEImageAsData image(
      reinterpret_cast<HMODULE>(image_mapping.data()));
  // PEImage class does not support other-architecture images. Skip over such
  // files.
  if (image.GetNTHeaders()->OptionalHeader.Magic !=
      IMAGE_NT_OPTIONAL_HDR_MAGIC) {
    return true;
  }

  // Explicitly update the last-modified time of the file if modifications are
  // successfully made. This is required to convince the Windows loader that the
  // modified file is distinct from the original. Windows does not necessarily
  // update the last-modified time of a file that is written to via a read-write
  // mapping.
  return upgrade_test::EnumResources(image, &VisitResource,
                                     reinterpret_cast<uintptr_t>(context)) &&
         UpdateVersionInData(&image, context) &&
         file.SetTimes(base::Time(), base::Time::Now());
}

bool UpdateManifestVersion(const base::FilePath& manifest,
                           VisitResourceContext* context) {
  std::string contents;
  if (!base::ReadFileToString(manifest, &contents)) {
    return false;
  }
  std::string old_version(context->current_version.ToASCII());
  std::string new_version(context->new_version.ToASCII());
  bool modified = false;
  if (!ReplaceAll(reinterpret_cast<uint8_t*>(&contents[0]),
                  reinterpret_cast<uint8_t*>(&contents[contents.size()]),
                  reinterpret_cast<uint8_t*>(&old_version[0]),
                  reinterpret_cast<uint8_t*>(&old_version[old_version.size()]),
                  reinterpret_cast<uint8_t*>(&new_version[0]), &modified)) {
    return false;
  }
  DCHECK(modified);
  return base::WriteFile(manifest, contents);
}

bool IncrementNewVersion(upgrade_test::Direction direction,
                         VisitResourceContext* ctx) {
  DCHECK(ctx);

  // Figure out a past or future version with the same string length as this one
  // by decrementing or incrementing each component.
  LONGLONG incrementer = (direction == upgrade_test::PREVIOUS_VERSION ? -1 : 1);

  do {
    if (incrementer == 0) {
      LOG(DFATAL) << "Improbable version at the cusp of complete rollover";
      return false;
    }
    ctx->new_version.set_value(ctx->current_version.value() + incrementer);
    ctx->new_version_str = ctx->new_version.ToString();
    incrementer <<= 16;
  } while (ctx->new_version_str.size() != ctx->current_version_str.size());

  return true;
}

// Raises or lowers the version of all .exe and .dll files in |work_dir| as well
// as the |work-dir|\Chrome-bin\w.x.y.z directory.  |original_version| and
// |new_version|, when non-null, are given the original and new version numbers
// on success.
bool ApplyAlternateVersion(const base::FilePath& work_dir,
                           upgrade_test::Direction direction,
                           std::wstring* original_version,
                           std::wstring* new_version) {
  VisitResourceContext ctx;
  if (!GetSetupExeVersion(work_dir, &ctx.current_version)) {
    return false;
  }
  ctx.current_version_str = ctx.current_version.ToString();

  if (!IncrementNewVersion(direction, &ctx)) {
    return false;
  }

  // Modify all .dll and .exe files with the current version.
  base::FileEnumerator all_files(work_dir, true, base::FileEnumerator::FILES);
  while (true) {
    base::FilePath file = all_files.Next();
    if (file.empty()) {
      break;
    }
    std::wstring extension = file.Extension();
    if ((extension == &kExtExe[0] || extension == &kExtDll[0]) &&
        !UpdateVersionIfMatch(file, &ctx)) {
      return false;
    }
  }

  // Change the versioned directory.
  base::FilePath chrome_bin = work_dir.Append(&kChromeBin[0]);
  if (!base::Move(chrome_bin.Append(ctx.current_version_str),
                  chrome_bin.Append(ctx.new_version_str))) {
    return false;
  }

  // Update the manifest (revise post-XP; see https://crbug.com/581133).
  base::FilePath current_manifest =
      chrome_bin.Append(ctx.new_version_str)
          .Append(ctx.current_version_str + L".manifest");
  if (base::PathExists(current_manifest)) {
    base::FilePath new_manifest =
        current_manifest.DirName().Append(ctx.new_version_str + L".manifest");
    if (!base::Move(current_manifest, new_manifest) ||
        !UpdateManifestVersion(new_manifest, &ctx)) {
      return false;
    }
  }

  // Report the version numbers if requested.
  if (original_version) {
    original_version->assign(ctx.current_version_str);
  }
  if (new_version) {
    new_version->assign(ctx.new_version_str);
  }

  return true;
}

// Returns the path to the directory holding the 7za executable.  By default, it
// is assumed that the test resides in the tree's output directory, so the
// relative path "..\..\third_party\lzma_sdk\Executable" is applied to the host
// executable's directory.  This can be overridden with the --7za_path
// command-line switch.
base::FilePath Get7zaPath() {
  base::FilePath l7za_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          &kSwitch7zaPath[0]);
  if (l7za_path.empty()) {
    base::FilePath dir_exe;
    if (!base::PathService::Get(base::DIR_EXE, &dir_exe)) {
      LOG(DFATAL) << "Failed getting directory of host executable";
    }
    l7za_path = dir_exe.Append(&k7zaPathRelative[0]);
  }
  return l7za_path;
}

bool CreateArchive(const base::FilePath& output_file,
                   const base::FilePath& input_path,
                   int compression_level) {
  DCHECK(compression_level == 0 || compression_level >= 1 &&
                                       compression_level <= 9 &&
                                       (compression_level & 0x01) != 0);

  std::wstring command_line(1, L'"');
  command_line.append(Get7zaPath().Append(&k7zaExe[0]).value())
      .append(L"\" a -bd -t7z \"")
      .append(output_file.value())
      .append(L"\" \"")
      .append(input_path.value())
      .append(L"\" -mx")
      .append(1, L'0' + compression_level);
  int exit_code;
  if (!RunProcessAndWait(nullptr, command_line, &exit_code)) {
    return false;
  }
  if (exit_code != 0) {
    LOG(DFATAL) << Get7zaPath().Append(&k7zaExe[0]).value()
                << " exited with code " << exit_code << " while creating "
                << output_file.value();
    return false;
  }
  return true;
}

}  // namespace

namespace upgrade_test {

bool GenerateAlternateVersion(const base::FilePath& original_installer_path,
                              const base::FilePath& target_path,
                              Direction direction,
                              std::wstring* original_version,
                              std::wstring* new_version) {
  // Create a temporary directory in which we'll do our work.
  ScopedTempDirectory work_dir;
  if (!work_dir.Initialize()) {
    return false;
  }

  // Copy the original mini_installer.
  base::FilePath mini_installer =
      work_dir.directory().Append(original_installer_path.BaseName());
  if (!base::CopyFile(original_installer_path, mini_installer)) {
    LOG(DFATAL) << "Failed copying \"" << original_installer_path.value()
                << "\" to \"" << mini_installer.value() << "\"";
    return false;
  }

  // Setup for compressed builds:
  base::FilePath setup_ex_;
  // Setup for uncompressed builds:
  base::FilePath setup_exe = work_dir.directory().Append(&kSetupExe[0]);
  const wchar_t* setup_resource_name = nullptr;
  base::FilePath chrome_packed_7z;  // Empty for uncompressed builds.
  base::FilePath chrome_7z;
  const wchar_t* archive_resource_name = nullptr;
  base::FilePath* archive_file = nullptr;

  // Load the original file and extract setup.ex_ and chrome.packed.7z
  {
    ResourceLoader resource_loader;
    std::pair<const uint8_t*, DWORD> resource_data;

    if (!resource_loader.Initialize(mini_installer)) {
      return false;
    }

    // Write out chrome.packed.7z (compressed builds) or chrome.7z (uncompressed
    // builds).
    if (resource_loader.Load(&kChromePacked7z[0], &kB7[0], &resource_data)) {
      archive_resource_name = &kChromePacked7z[0];
      chrome_packed_7z = work_dir.directory().Append(archive_resource_name);
      archive_file = &chrome_packed_7z;
    } else if (resource_loader.Load(&kChrome7z[0], &kBN[0], &resource_data)) {
      archive_resource_name = &kChrome7z[0];
      chrome_7z = work_dir.directory().Append(archive_resource_name);
      archive_file = &chrome_7z;
    } else {
      return false;
    }
    DCHECK(archive_resource_name);
    DCHECK(!chrome_packed_7z.empty() || !chrome_7z.empty());
    DCHECK(archive_file);
    if (!base::WriteFile(
            *archive_file,
            base::make_span(resource_data.first, resource_data.second))) {
      LOG(DFATAL) << "Failed writing \"" << archive_file->value() << "\"";
      return false;
    }

    // Write out setup.ex_ (compressed builds) or
    // setup.exe (uncompressed builds).
    if (resource_loader.Load(&kSetupEx_[0], &kBl[0], &resource_data)) {
      setup_ex_ = work_dir.directory().Append(&kSetupEx_[0]);
      setup_resource_name = &kSetupEx_[0];
      if (!base::WriteFile(setup_ex_, base::make_span(resource_data.first,
                                                      resource_data.second))) {
        LOG(DFATAL) << "Failed writing \"" << setup_ex_.value() << "\"";
        return false;
      }
    } else if (resource_loader.Load(&kSetupExe[0], &kBN[0], &resource_data)) {
      setup_resource_name = &kSetupExe[0];
      if (!base::WriteFile(setup_exe, base::make_span(resource_data.first,
                                                      resource_data.second))) {
        LOG(DFATAL) << "Failed writing \"" << setup_exe.value() << "\"";
        return false;
      }
    } else {
      return false;
    }
  }

  // If a compressed setup was found..
  if (!setup_ex_.empty()) {
    // Expand setup.ex_
    setup_exe = setup_ex_.ReplaceExtension(&kExe[0]);
    std::wstring command_line(1, L'"');
    command_line.append(&kExpandExe[0])
        .append(L"\" \"")
        .append(setup_ex_.value())
        .append(L"\" \"")
        .append(setup_exe.value())
        .append(1, L'\"');
    int exit_code = 0;
    if (!RunProcessAndWait(nullptr, command_line, &exit_code)) {
      return false;
    }
    if (exit_code != 0) {
      LOG(DFATAL) << &kExpandExe[0] << " exited with code " << exit_code;
      return false;
    }
  }

  // Unpack chrome.packed.7z (compressed builds only).
  if (!chrome_packed_7z.empty()) {
    if (UnPackArchive(chrome_packed_7z, work_dir.directory(), &chrome_7z) !=
        UNPACK_NO_ERROR) {
      LOG(DFATAL) << "Failed unpacking \"" << chrome_packed_7z.value() << "\"";
      return false;
    }
  }
  DCHECK(!chrome_7z.empty());

  // Unpack chrome.7z
  if (UnPackArchive(chrome_7z, work_dir.directory(), /*output_file=*/nullptr) !=
      UNPACK_NO_ERROR) {
    LOG(DFATAL) << "Failed unpacking \"" << chrome_7z.value() << "\"";
    return false;
  }

  // Get rid of intermediate files
  if (!base::DeleteFile(chrome_7z) ||
      (!chrome_packed_7z.empty() && !base::DeleteFile(chrome_packed_7z)) ||
      !base::DeleteFile(setup_ex_)) {
    LOG(DFATAL) << "Failed deleting intermediate files";
    return false;
  }
  // Increment the version in all files.
  ApplyAlternateVersion(work_dir.directory(), direction, original_version,
                        new_version);

  // Pack up files into chrome.7z
  if (!CreateArchive(chrome_7z, work_dir.directory().Append(&kChromeBin[0]),
                     0)) {
    return false;
  }

  // Compress chrome.7z into chrome.packed.7z for compressed builds.
  if (!chrome_packed_7z.empty() &&
      !CreateArchive(chrome_packed_7z, chrome_7z, 9)) {
    return false;
  }

  // Compress setup.exe into setup.ex_ for compressed builds.
  if (!setup_ex_.empty()) {
    std::wstring command_line(1, L'"');
    command_line.append(&kMakeCab[0])
        .append(L"\" /D CompressionType=LZX /L \"")
        .append(work_dir.directory().value())
        .append(L"\" \"")
        .append(setup_exe.value());
    int exit_code = 0;
    if (!RunProcessAndWait(nullptr, command_line, &exit_code)) {
      return false;
    }
    if (exit_code != 0) {
      LOG(DFATAL) << &kMakeCab[0] << " exited with code " << exit_code;
      return false;
    }
  }

  // Replace the mini_installer's setup.ex_ (or setup.exe for an uncompressed
  // mini_installer) and chrome.packed.7z (or chrome.7z in component and
  // non-debug builds) resources.
  ResourceUpdater updater;
  if (!updater.Initialize(mini_installer) ||
      !updater.Update(setup_resource_name,
                      (!setup_ex_.empty() ? &kBl[0] : &kBN[0]),
                      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                      (!setup_ex_.empty() ? setup_ex_ : setup_exe)) ||
      !updater.Update(archive_resource_name,
                      (!chrome_packed_7z.empty() ? &kB7[0] : &kBN[0]),
                      MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                      *archive_file) ||
      !updater.Commit()) {
    LOG(ERROR) << "It is common for this step to fail for very large resources,"
                  " as is the case for Debug component=shared_library builds. "
                  "Try with a Release or component=static_library build.";
    return false;
  }

  // Finally, move the updated mini_installer into place.
  return base::Move(mini_installer, target_path);
}

std::wstring GenerateAlternatePEFileVersion(const base::FilePath& original_file,
                                            const base::FilePath& target_file,
                                            Direction direction) {
  VisitResourceContext ctx;
  if (!GetFileVersion(original_file, &ctx.current_version)) {
    LOG(DFATAL) << "Failed reading version from \"" << original_file.value()
                << "\"";
    return std::wstring();
  }
  ctx.current_version_str = ctx.current_version.ToString();

  if (!IncrementNewVersion(direction, &ctx)) {
    LOG(DFATAL) << "Failed to increment version from \""
                << original_file.value() << "\"";
    return std::wstring();
  }

  DCHECK_EQ(ctx.current_version_str.size(), ctx.new_version_str.size());

  if (!base::CopyFile(original_file, target_file)) {
    LOG(DFATAL) << "Failed copying \"" << original_file.value() << "\" to \""
                << target_file.value() << "\"";
    return std::wstring();
  }

  if (!UpdateVersionIfMatch(target_file, &ctx)) {
    return std::wstring();
  }

  return ctx.new_version_str;
}

}  // namespace upgrade_test
