// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/disk_util.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/file_version_info.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/current_module.h"
#include "base/win/registry.h"
#include "base/win/shortcut.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/constants/version.h"
#include "chrome/chrome_cleaner/os/file_path_sanitization.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"
#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"
#include "chrome/chrome_cleaner/os/scoped_service_handle.h"
#include "chrome/chrome_cleaner/os/system_util.h"
#include "chrome/chrome_cleaner/strings/string_util.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"

namespace chrome_cleaner {

namespace {

using crypto::SecureHash;

// The name of the registry value where the 64 bits ProgramFiles path can be
// read from.
const wchar_t kProgramFilesDirValueName[] = L"ProgramFilesDir";

// The recommended buffer size for efficient file reads.
const size_t kReadBufferSize = 4 * 1024;

// The registry key where the ProgramFilesDir value can be read from.
const wchar_t kWindowsCurrentVersionRegKeyName[] =
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion";

// No CSIDL constant exists to find the Common Files folder in Program Files
// x64. Use the %CommonProgramW6432% environment instead. Should be set to
// "C:\Program Files\Common Files\".
const wchar_t kCommonProgramW6432[] = L"%CommonProgramW6432%";

constexpr const base::char16* kCompanyIgnoredReportingList[] = {
    STRING16_LITERAL("Google LLC"),
    STRING16_LITERAL("Google Inc"),
    STRING16_LITERAL("Google Inc."),
    STRING16_LITERAL("Intel Corporation"),
    STRING16_LITERAL("Microsoft Corporation"),
};

// Built from various sources to try and include all the extensions that are
// used by active parts of UwS installations.
const wchar_t* const kActiveExtensions[] = {
    L".bat", L".bin", L".cfg", L".class", L".cmd", L".com", L".cpl", L".crx",
    L".dat", L".db", L".dll", L".drv", L".exe", L".gadget", L".grp", L".inf",
    L".ins", L".inx", L".isu", L".jar", L".jnlp", L".job", L".js", L".jse",
    L".mof", L".msc", L".msi", L".msp", L".mst", L".ocx", L".pac", L".paf",
    L".pif", L".ps1", L".peg", L".py", L".rb", L".rgs", L".sct", L".spl",
    L".swf", L".sys", L".shb", L".u3p", L".vb", L".vbe", L".vbs", L".vbscript",
    L".ws", L".wsf", L".xbap", L".xhtm5",
    // Empty extensions should be view as active.
    L"",
    // Shortcuts might lead to active files; allow their deletion.
    L".lnk",
    // ConvertAd has services that use non-standard extensions. Mark these
    // extensions as active to ensure we can delete these services.
    L".tmp", L".tmpfs",
};

// An easier to search set of the extensions above.
ExtensionSet g_active_extensions;

constexpr wchar_t kDefaultDataStream[] = L"::$DATA";

// Collect path from |root_path| matching a path using wildcards in |components|
// starting from index |component_index|. The matched paths are added to
// |matches|. This algorithm is a depth-first recursive enumeration of files and
// folders. A depth first search is needed to avoid consuming large amount of
// memory space when visiting the files system (i.e. program files/*/*/*/a.exe).
void CollectMatchingPathsRecursive(
    const base::FilePath& root_path,
    const std::vector<base::FilePath::StringType>& components,
    size_t component_index,
    std::vector<base::FilePath>* matches) {
  DCHECK(matches);
  if (components.size() == component_index) {
    if (base::PathExists(root_path))
      matches->push_back(root_path);
    return;
  }

  const auto& component = components[component_index];
  if (PathContainsWildcards(base::FilePath(component))) {
    // The current component contains wild-card characters.
    base::FileEnumerator file_enum(
        root_path, false,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES,
        component);
    for (base::FilePath file = file_enum.Next(); !file.empty();
         file = file_enum.Next()) {
      // The base file name needs to be matched again with the pattern even if
      // the |FileEnumerator| has the same pattern specified. On windows, the
      // way these pattern are expanded will match the same files: "*", "*.*",
      // "*.*.*". The |NameMatchesPattern| will enforce a strict pattern match.
      if (NameMatchesPattern(file.BaseName().value(), component, 0)) {
        CollectMatchingPathsRecursive(file, components, component_index + 1,
                                      matches);
      }
    }
  } else {
    // The current component doesn't contain wild-card characters.
    CollectMatchingPathsRecursive(root_path.Append(component), components,
                                  component_index + 1, matches);
  }
}

void AppendFileInformationField(const wchar_t* field_name,
                                const base::string16& field,
                                base::string16* information) {
  DCHECK(field_name);
  DCHECK(information);
  if (!field.empty()) {
    *information += L", ";
    *information += field_name;
    *information += L" = '" + field + L"'";
  }
}

bool ExpandEnvPathandWow64PathIfFileExists(
    const base::FilePath& program_path,
    base::FilePath* return_program_path) {
  DCHECK(return_program_path);
  if (base::PathExists(program_path) && !base::DirectoryExists(program_path)) {
    *return_program_path = program_path;
    return true;
  }
  base::FilePath expanded_program_path =
      ExpandEnvPathAndWow64Path(program_path);
  if (base::PathExists(expanded_program_path) &&
      !base::DirectoryExists(expanded_program_path)) {
    *return_program_path = expanded_program_path;
    return true;
  }
  return false;
}

bool ExtractExecutablePathWithoutArgument(const base::FilePath& program_path,
                                          base::FilePath* return_program_path) {
  // Find the file path end point without quote since base::CommandLine doesn't
  // support it.
  // e.g.
  //  "C:\Program Files\command.exe" -s  (With quotes, supported by
  //  base::CommandLine.
  //     -> C:\Program Files\command.exe
  //  C:\Program Files\test.exe -s   (Without quotes, supported by this
  //  function.)
  // -> C:\Program (not a valid file)
  // -> C:\Program Files\test.exe (return as a valid file)
  DCHECK(return_program_path);
  if (ExpandEnvPathandWow64PathIfFileExists(program_path,
                                            return_program_path)) {
    return true;
  }
  size_t program_path_length = program_path.value().find(L" ");
  while (program_path_length != base::string16::npos) {
    base::FilePath truncated_path(
        program_path.value().substr(0, program_path_length));
    if (ExpandEnvPathandWow64PathIfFileExists(truncated_path,
                                              return_program_path)) {
      return true;
    }
    program_path_length =
        program_path.value().find(L" ", program_path_length + 1);
  }
  return false;
}

bool IsActionRunDll32(const base::FilePath& exec_path) {
  return String16EqualsCaseInsensitive(
      exec_path.BaseName().RemoveExtension().value(), L"rundll32");
}

base::FilePath ExtractRunDllTargetPath(const base::string16& arguments) {
  // Some programs use rundll instead of an executable, and so their disk
  // footprint will be the first of a set of comma separated list of
  // arguments passed to rundll32.exe, which may also be "quoted", and may
  // also have command line arguments. We can't use CommandLine::GetArgs nor
  // CommandLine::GetArgumentsString() since they split/quote by spaces.
  std::vector<base::string16> rundll_args = base::SplitString(
      arguments, L",\"", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (rundll_args.empty()) {
    LOG(WARNING) << "Rundll without any arguments? '" << arguments << "'";
    return base::FilePath();
  }

  return base::FilePath(rundll_args[0]);
}

// Return a date string formatted as "YYYY-MM-DD".
std::string TimeFormatDate(const base::Time& time) {
  base::Time::Exploded exploded_time;
  time.UTCExplode(&exploded_time);
  return base::StringPrintf("%04d-%02d-%02d", exploded_time.year,
                            exploded_time.month, exploded_time.day_of_month);
}

base::FilePath AppendProductPath(const base::FilePath& base_path) {
  std::unique_ptr<FileVersionInfo> file_version_info(
      FileVersionInfo::CreateFileVersionInfoForModule(CURRENT_MODULE()));

  if (file_version_info.get()) {
    return base_path.Append(file_version_info->company_short_name())
        .Append(file_version_info->product_short_name());
  } else {
    return base_path.Append(COMPANY_SHORTNAME_STRING)
        .Append(L"Chrome Cleanup Tool Test");
  }
}

}  // namespace

base::FilePath GetX64ProgramFilesPath(const base::FilePath& input_path) {
  // On X86 system, there is no X64 program files folder, returns empty path.
  if (base::win::OSInfo::GetArchitecture() ==
      base::win::OSInfo::X86_ARCHITECTURE) {
    return base::FilePath();
  }
  base::win::RegKey version_key(HKEY_LOCAL_MACHINE,
                                kWindowsCurrentVersionRegKeyName,
                                KEY_READ | KEY_WOW64_64KEY);
  DCHECK(version_key.Valid());
  base::string16 program_files_path;
  LONG error =
      version_key.ReadValue(kProgramFilesDirValueName, &program_files_path);
  if (error != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to read " << kProgramFilesDirValueName << " value "
                << "from " << kWindowsCurrentVersionRegKeyName << ", " << error;
    return base::FilePath();
  }
  return base::FilePath(program_files_path).Append(input_path);
}

base::FilePath GetX86ProgramFilesPath(const base::FilePath& input_path) {
  base::win::RegKey version_key(HKEY_LOCAL_MACHINE,
                                kWindowsCurrentVersionRegKeyName,
                                KEY_READ | KEY_WOW64_32KEY);
  DCHECK(version_key.Valid());
  base::string16 program_files_path;
  LONG error =
      version_key.ReadValue(kProgramFilesDirValueName, &program_files_path);
  if (error != ERROR_SUCCESS) {
    PLOG(ERROR) << "Failed to read " << kProgramFilesDirValueName << " value "
                << "from " << kWindowsCurrentVersionRegKeyName << ", " << error;
    return base::FilePath();
  }
  return base::FilePath(program_files_path).Append(input_path);
}

bool NameContainsWildcards(const base::string16& name) {
  return (name.find(L"*") != base::FilePath::StringType::npos ||
          name.find(L"?") != base::FilePath::StringType::npos);
}

bool NameMatchesPattern(const base::string16& name,
                        const base::string16& pattern,
                        const wchar_t escape_char) {
  return String16WildcardMatchInsensitive(name, pattern, escape_char);
}

void CollectMatchingPaths(const base::FilePath& root_path,
                          std::vector<base::FilePath>* matches) {
  DCHECK(matches);

  if (PathContainsWildcards(root_path)) {
    std::vector<base::FilePath::StringType> components;
    root_path.GetComponents(&components);
    base::FilePath empty_path;
    CollectMatchingPathsRecursive(empty_path, components, 0, matches);
  } else if (base::PathExists(root_path)) {
    matches->push_back(root_path);
  }
}

bool PathContainsWildcards(const base::FilePath& file_path) {
  base::FilePath::StringType name(file_path.value());
  return NameContainsWildcards(name);
}

bool PathHasActiveExtension(const base::FilePath& file_path) {
  base::string16 extension;
  if (base::EndsWith(file_path.value(), kDefaultDataStream,
                     base::CompareCase::INSENSITIVE_ASCII)) {
    // Default stream with an explicit stream type specified.
    // The type of the default stream should be $DATA, in which case let's
    // remove the whole stream specification from the path and do extension
    // check.
    size_t true_path_len =
        file_path.value().size() - wcslen(kDefaultDataStream);
    base::string16 true_path = file_path.value().substr(0, true_path_len);
    extension = base::FilePath(true_path).Extension();
  } else {
    CHECK_EQ(base::FilePath::StringType::npos,
             file_path.BaseName().value().find(L"::"))
        << "Stream type other than $DATA was specified for the default stream: "
        << file_path.BaseName().value();
    extension = file_path.Extension();
  }
  base::TrimString(extension, L" ", &extension);
  return g_active_extensions.find(extension.c_str()) !=
         g_active_extensions.end();
}

void InitializeDiskUtil() {
  // Only do this once.
  static bool init_once = []() -> bool {
    // Initialize the binary extension, so it can be used from different threads
    // without the initial creation race.
    DCHECK(g_active_extensions.empty());
    for (const wchar_t* const extension : kActiveExtensions) {
      g_active_extensions.insert(extension);
    }
    DCHECK(!g_active_extensions.empty());
    return true;
  }();
  ANALYZER_ALLOW_UNUSED(init_once);
}

bool ExpandEnvPath(const base::FilePath& path, base::FilePath* expanded_path) {
  DCHECK(expanded_path);
  const base::FilePath::StringType unexpanded_value = path.value();

  // |ExpandEnvironmentStrings| will return the number of characters required
  // when called with a buffer too small to hold the result.
  const DWORD required_size =
      ::ExpandEnvironmentStrings(unexpanded_value.c_str(), nullptr, 0);
  if (!required_size) {
    PLOG(ERROR) << "ExpandEnvironmentStrings failed";
    return false;
  }

  // Allocate a buffer large enough for the expanded path, and expand it into
  // that buffer. MSDN says that |ExpandEnvironmentStrings| returns the number
  // of characters required (including the null terminating character) if the
  // buffer is too small for the result (which is the case above), and that
  // the buffer should be one character longer than that. If the buffer is large
  // enough, the function will return the number of characters actually used.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms724265(v=vs.85).aspx
  std::vector<base::FilePath::StringType::value_type> expanded_value(
      required_size + 1, '\0');
  const DWORD expanded_size = ::ExpandEnvironmentStrings(
      unexpanded_value.c_str(), &expanded_value[0], expanded_value.size());
  DCHECK_EQ(required_size, expanded_size);

  // Create a new FilePath object from the expanded path.
  *expanded_path = base::FilePath(
      base::FilePath::StringPieceType(&expanded_value[0], expanded_size));

  return true;
}

void ExpandWow64Path(const base::FilePath& path,
                     base::FilePath* expanded_path) {
  DCHECK(expanded_path);
  base::FilePath system_path =
      PreFetchedPaths::GetInstance()->GetCsidlSystemFolder();

  *expanded_path = path;

  base::FilePath native_path = system_path.DirName().Append(L"sysnative");
  if (system_path.IsParent(path) &&
      system_path.AppendRelativePath(path, &native_path) &&
      base::PathExists(native_path)) {
    *expanded_path = native_path;
  }
}

base::string16 FileInformationToString(
    const internal::FileInformation& file_information) {
  if (file_information.path.empty())
    return L"";

  // We add the first field directly without using any AppendFileInformation*()
  // function since the first field should not be prepended with a separator.
  base::string16 content = L"path = '" + file_information.path + L"'";

  AppendFileInformationField(L"file_creation_date",
                             base::UTF8ToUTF16(file_information.creation_date),
                             &content);
  AppendFileInformationField(
      L"file_last_modified_date",
      base::UTF8ToUTF16(file_information.last_modified_date), &content);
  AppendFileInformationField(
      L"digest", base::UTF8ToUTF16(file_information.sha256), &content);
  AppendFileInformationField(
      L"size", base::NumberToString16(file_information.size), &content);
  AppendFileInformationField(L"company_name", file_information.company_name,
                             &content);
  AppendFileInformationField(L"company_short_name",
                             file_information.company_short_name, &content);
  AppendFileInformationField(L"product_name", file_information.product_name,
                             &content);
  AppendFileInformationField(L"product_short_name",
                             file_information.product_short_name, &content);
  AppendFileInformationField(L"internal_name", file_information.internal_name,
                             &content);
  AppendFileInformationField(L"original_filename",
                             file_information.original_filename, &content);
  AppendFileInformationField(L"file_description",
                             file_information.file_description, &content);
  AppendFileInformationField(L"file_version", file_information.file_version,
                             &content);
  AppendFileInformationField(
      L"active_file", base::NumberToString16(file_information.active_file),
      &content);

  return content;
}

bool IsCompanyOnIgnoredReportingList(const base::string16& company_name) {
  return base::Contains(kCompanyIgnoredReportingList, company_name);
}

bool IsExecutableOnIgnoredReportingList(const base::FilePath& file_path) {
  std::unique_ptr<FileVersionInfo> file_information(
      FileVersionInfo::CreateFileVersionInfo(file_path));
  return file_information &&
         IsCompanyOnIgnoredReportingList(file_information->company_name());
}

bool RetrieveDetailedFileInformation(
    const base::FilePath& file_path,
    internal::FileInformation* file_information,
    bool* ignored_reporting,
    IgnoredReportingCallback ignored_reporting_callback) {
  DCHECK(file_information);
  DCHECK(ignored_reporting);

  base::FilePath expanded_path;
  if (!TryToExpandPath(file_path, &expanded_path))
    return false;

  if (std::move(ignored_reporting_callback).Run(file_path)) {
    *ignored_reporting = true;
    return false;
  }
  *ignored_reporting = false;

  // Retrieve the basic file information.
  RetrievePathInformation(expanded_path, file_information);

  // Retrieve the detailed file information.
  if (!ComputeSHA256DigestOfPath(expanded_path, &file_information->sha256)) {
    LOG(ERROR) << "Unable to compute digest SHA256 for: '"
               << file_information->path << "'";
    return false;
  }

  // Set the executable version information, when available.
  std::unique_ptr<FileVersionInfo> version(
      FileVersionInfo::CreateFileVersionInfo(expanded_path));
  if (version) {
    file_information->company_name = version->company_name();
    file_information->company_short_name = version->company_short_name();
    file_information->product_name = version->product_name();
    file_information->product_short_name = version->product_short_name();
    file_information->internal_name = version->internal_name();
    file_information->original_filename = version->original_filename();
    file_information->file_description = version->file_description();
    file_information->file_version = version->file_version();
  }

  return true;
}

bool RetrieveBasicFileInformation(const base::FilePath& file_path,
                                  internal::FileInformation* file_information) {
  DCHECK(file_information);
  base::FilePath expanded_path;
  if (!TryToExpandPath(file_path, &expanded_path))
    return false;
  RetrievePathInformation(expanded_path, file_information);

  return true;
}

bool RetrieveFileInformation(const base::FilePath& file_path,
                             bool include_details,
                             internal::FileInformation* file_information) {
  if (include_details) {
    bool ignored_reporting_unused = false;
    return RetrieveDetailedFileInformation(file_path, file_information,
                                           &ignored_reporting_unused);
  } else {
    return RetrieveBasicFileInformation(file_path, file_information);
  }
}

bool ComputeSHA256DigestOfPath(const base::FilePath& path,
                               std::string* digest) {
  DCHECK(digest);

  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_SHARE_DELETE);
  if (!file.IsValid())
    return false;

  std::unique_ptr<SecureHash> ctx(SecureHash::Create(SecureHash::SHA256));
  char buffer[kReadBufferSize];
  while (true) {
    int count = file.ReadAtCurrentPos(buffer, kReadBufferSize);
    if (count <= 0)
      break;
    ctx->Update(buffer, count);
  }

  char digest_bytes[crypto::kSHA256Length];
  ctx->Finish(digest_bytes, crypto::kSHA256Length);

  *digest = base::HexEncode(digest_bytes, crypto::kSHA256Length);
  return true;
}

bool ComputeSHA256DigestOfString(const std::string& content,
                                 std::string* digest) {
  DCHECK(digest);

  std::unique_ptr<SecureHash> ctx(SecureHash::Create(SecureHash::SHA256));

  ctx->Update(content.c_str(), content.length());

  char digest_bytes[crypto::kSHA256Length];
  ctx->Finish(digest_bytes, crypto::kSHA256Length);

  *digest = base::HexEncode(digest_bytes, crypto::kSHA256Length);
  return true;
}

bool GUIDLess::operator()(const GUID& smaller, const GUID& larger) const {
  if (smaller.Data1 < larger.Data1)
    return true;
  if (smaller.Data1 > larger.Data1)
    return false;
  if (smaller.Data2 < larger.Data2)
    return true;
  if (smaller.Data2 > larger.Data2)
    return false;
  if (smaller.Data3 < larger.Data3)
    return true;
  if (smaller.Data3 > larger.Data3)
    return false;
  for (size_t i = 0; i < 8; ++i) {
    if (smaller.Data4[i] < larger.Data4[i])
      return true;
    if (smaller.Data4[i] > larger.Data4[i])
      return false;
  }
  // Equality means not less, so false.
  return false;
}

void GetLayeredServiceProviders(const LayeredServiceProviderAPI& lsp_api,
                                LSPPathToGUIDs* providers) {
  DCHECK(providers);

  // Find out how much memory is needed. If we get the expected error, the
  // memory needed is written to size.
  DWORD size = 0;
  int error = 0;
  int num_service_providers =
      lsp_api.EnumProtocols(nullptr, nullptr, &size, &error);
  if (num_service_providers == 0) {
    DCHECK_EQ(0UL, size);
    VLOG(1) << "No registered LSP found.";
    return;
  }

  // We expect an error, when the memory needed is written to size > 0.
  DCHECK_GT(size, 0UL);
  if (num_service_providers != SOCKET_ERROR || error != WSAENOBUFS) {
    NOTREACHED() << "Failed to get the number of protocols. " << error;
    return;
  }

  std::unique_ptr<char[]> service_provider_bytes(new char[size]);
  WSAPROTOCOL_INFOW* service_providers =
      reinterpret_cast<WSAPROTOCOL_INFOW*>(service_provider_bytes.get());
  num_service_providers =
      lsp_api.EnumProtocols(nullptr, service_providers, &size, &error);
  if (num_service_providers == SOCKET_ERROR) {
    NOTREACHED() << "Failed to get the list of protocols. " << error;
    return;
  }

  for (int i = 0; i < num_service_providers; ++i) {
    wchar_t path[MAX_PATH];
    int path_length = base::size(path);
    if (0 == lsp_api.GetProviderPath(&service_providers[i].ProviderId, path,
                                     &path_length, &error)) {
      std::pair<LSPPathToGUIDs::iterator, bool> inserted =
          providers->emplace(base::FilePath(path), std::set<GUID, GUIDLess>());
      inserted.first->second.insert(service_providers[i].ProviderId);
    } else {
      LOG(ERROR) << "Couldn't get path for provider: "
                 << base::win::String16FromGUID(
                        service_providers[i].ProviderId);
    }
  }
}

// Code adapted from:
// https://cs.chromium.org/chromium/src/chrome/installer/setup/setup_util.cc
bool DeleteFileFromTempProcess(const base::FilePath& path,
                               uint32_t delay_before_delete_ms,
                               base::win::ScopedHandle* process_handle) {
  // No need to delete an inexistent file.
  if (path.empty() || !base::PathExists(path))
    return false;

  static const wchar_t kRunDll32Path[] =
      L"%SystemRoot%\\System32\\rundll32.exe";
  wchar_t rundll32[MAX_PATH] = {};
  DWORD size =
      ExpandEnvironmentStrings(kRunDll32Path, rundll32, base::size(rundll32));
  if (!size || size >= MAX_PATH)
    return false;

  STARTUPINFO startup = {sizeof(STARTUPINFO)};
  PROCESS_INFORMATION pi = {0};
  BOOL ok = ::CreateProcess(nullptr, rundll32, nullptr, nullptr, FALSE,
                            CREATE_SUSPENDED, nullptr, nullptr, &startup, &pi);
  if (!ok) {
    PLOG(ERROR) << "CreateProcess: " << rundll32;
    return false;
  }
  base::win::ScopedHandle process(pi.hProcess);
  base::win::ScopedHandle thread(pi.hThread);

  // We use the main thread of the new process to run:
  //   Sleep(delay_before_delete_ms);
  //   DeleteFile(path);
  //   ExitProcess(0);
  // This runs before the main routine of the process runs, so it doesn't
  // matter much which executable we choose except that we don't want to
  // use e.g. a console app that causes a window to be created.
  size = (path.value().length() + 1) * sizeof(path.value()[0]);
  void* path_in_other_process =
      ::VirtualAllocEx(pi.hProcess, nullptr, size, MEM_COMMIT, PAGE_READWRITE);
  if (!path_in_other_process) {
    PLOG(ERROR) << "VirtualAllocEx";
    ::TerminateProcess(pi.hProcess, ~static_cast<UINT>(0));
    return false;
  }
  SIZE_T written = 0;
  ::WriteProcessMemory(pi.hProcess, path_in_other_process, path.value().c_str(),
                       (path.value().size() + 1) * sizeof(path.value()[0]),
                       &written);
  HMODULE kernel32 = ::GetModuleHandle(L"kernel32.dll");
  PAPCFUNC sleep =
      reinterpret_cast<PAPCFUNC>(::GetProcAddress(kernel32, "Sleep"));
  PAPCFUNC delete_file =
      reinterpret_cast<PAPCFUNC>(::GetProcAddress(kernel32, "DeleteFileW"));
  PAPCFUNC exit_process =
      reinterpret_cast<PAPCFUNC>(::GetProcAddress(kernel32, "ExitProcess"));
  if (!sleep || !delete_file || !exit_process) {
    NOTREACHED();
    ok = FALSE;
  } else {
    ::QueueUserAPC(sleep, pi.hThread, delay_before_delete_ms);
    ::QueueUserAPC(delete_file, pi.hThread,
                   reinterpret_cast<ULONG_PTR>(path_in_other_process));
    ::QueueUserAPC(exit_process, pi.hThread, 0);
    ::ResumeThread(pi.hThread);
  }
  if (ok && process_handle)
    process_handle->Set(process.Take());

  return ok != FALSE;
}

bool PathEqual(const base::FilePath& path1, const base::FilePath& path2) {
  base::string16 long_path1;
  base::string16 long_path2;
  ConvertToLongPath(path1.value(), &long_path1);
  ConvertToLongPath(path2.value(), &long_path2);
  return base::FilePath::CompareEqualIgnoreCase(long_path1, long_path2);
}

bool FilePathLess::operator()(const base::FilePath& smaller,
                              const base::FilePath& larger) const {
  base::string16 long_smaller;
  base::string16 long_larger;
  ConvertToLongPath(smaller.value(), &long_smaller);
  ConvertToLongPath(larger.value(), &long_larger);
  return base::FilePath::CompareLessIgnoreCase(long_smaller, long_larger);
}

bool GetAppDataProductDirectory(base::FilePath* path) {
  DCHECK(path);
  base::FilePath app_data_path;
  if (!base::PathService::Get(CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
                              &app_data_path) &&
      // This has to be here because ScopedLogging can call this function before
      // the CSIDL based PathService handler is registered.
      !base::PathService::Get(base::DIR_LOCAL_APP_DATA, &app_data_path)) {
    LOG(ERROR) << "Can't retrieve local app data directory.";
    return false;
  }

  const base::FilePath product_app_data_path = AppendProductPath(app_data_path);
  if (!base::CreateDirectory(product_app_data_path)) {
    LOG(ERROR) << "Can't create product directory.";
    return false;
  }

  *path = product_app_data_path;
  return true;
}

void GetProgramFilesFolders(std::set<base::FilePath>* folders) {
  static const unsigned int kProgramFilesFolders[] = {
      // See the CSIDL_PROGRAM_FILES comment for rewrite_rules[].
      CsidlToPathServiceKey(CSIDL_PROGRAM_FILES),
      CsidlToPathServiceKey(CSIDL_PROGRAM_FILESX86),
      base::DIR_PROGRAM_FILES6432,
  };

  DCHECK(folders);
  for (unsigned int program_path : kProgramFilesFolders) {
    base::FilePath programfiles_folder;
    if (!base::PathService::Get(program_path, &programfiles_folder)) {
      LOG(ERROR) << "Can't get path from PathService.";
      continue;
    }
    folders->insert(programfiles_folder);
  }
}

void GetProgramFilesCommonFolders(std::set<base::FilePath>* folders) {
  static const unsigned int kCsidlProgramFileFolders[] = {
      CSIDL_PROGRAM_FILES_COMMONX86, CSIDL_PROGRAM_FILES_COMMON,
  };
  DCHECK(folders);
  // The CSIDL_PROGRAM_FILES_COMMON has no equivalent in the PathService. The
  // standard windows API is used to expand these paths.
  for (unsigned int program_path : kCsidlProgramFileFolders) {
    base::FilePath programfiles_folder =
        ExpandSpecialFolderPath(program_path, base::FilePath());
    if (programfiles_folder.empty()) {
      LOG(ERROR) << "Can't get path from ExpandSpecialFolderPath.";
      continue;
    }
    folders->insert(programfiles_folder);
  }

  // No CSIDL constant exists to find the Common Files folder in Program Files
  // x64. Use the %CommonProgramW6432% environment instead.
  base::FilePath common_program_env(kCommonProgramW6432);
  base::FilePath common_files_x6432_folder;
  if (ExpandEnvPath(common_program_env, &common_files_x6432_folder)) {
    folders->insert(common_files_x6432_folder);
  } else if (base::win::OSInfo::GetInstance()->wow64_status() ==
             base::win::OSInfo::WOW64_ENABLED) {
    LOG(ERROR) << "Can't get path for %CommonProgramW6432%";
  }
}

void GetAllProgramFolders(std::set<base::FilePath>* folders) {
  static const unsigned int kProgramFilesFolders[] = {
      CsidlToPathServiceKey(CSIDL_APPDATA),
      CsidlToPathServiceKey(CSIDL_LOCAL_APPDATA),
      CsidlToPathServiceKey(CSIDL_COMMON_APPDATA),
  };

  DCHECK(folders);
  for (unsigned int program_path : kProgramFilesFolders) {
    base::FilePath programfiles_folder;
    if (!base::PathService::Get(program_path, &programfiles_folder)) {
      LOG(ERROR) << "Can't get path from PathService.";
      continue;
    }
    folders->insert(programfiles_folder);
  }
  GetProgramFilesFolders(folders);
  GetProgramFilesCommonFolders(folders);
}

bool HasZoneIdentifier(const base::FilePath& path) {
  base::FilePath stream_path(path.value() + L":Zone.Identifier");
  return base::PathExists(stream_path);
}

bool OverwriteZoneIdentifier(const base::FilePath& path) {
  const DWORD kShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
  base::string16 stream_path = path.value() + L":Zone.Identifier";
  HANDLE file = CreateFile(stream_path.c_str(), GENERIC_WRITE, kShare, nullptr,
                           OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (INVALID_HANDLE_VALUE == file)
    return false;

  static const char kIdentifier[] = "[ZoneTransfer]\r\nZoneId=0\r\n";
  // Don't include trailing null in data written.
  static const DWORD kIdentifierSize = base::size(kIdentifier) - 1;
  DWORD written = 0;
  BOOL result =
      WriteFile(file, kIdentifier, kIdentifierSize, &written, nullptr);
  BOOL flush_result = FlushFileBuffers(file);
  CloseHandle(file);

  if (!result || !flush_result || written != kIdentifierSize) {
    NOTREACHED();
    return false;
  }

  return true;
}

base::FilePath ExtractExecutablePathFromRegistryContent(
    const base::string16& content) {
  // The content of the registry key can be a fullpath to an executable as is.
  base::FilePath program_path(content);
  base::FilePath return_program_path;
  if (ExpandEnvPathandWow64PathIfFileExists(program_path, &return_program_path))
    return return_program_path;

  // When the content is a command-line, the program name cannot contain spaces
  // or is quoted. In the case of an executable path, the path may contain
  // spaces and cannot be parse by |base::CommandLine::FromString| which
  // incorrectly splits the content around spaces (e.g.,
  // c:\Program Files\appname\dummy.exe).
  program_path = base::CommandLine::FromString(content).GetProgram();
  if (IsActionRunDll32(program_path)) {
    program_path =
        ExtractRunDllTargetPath(content.substr(program_path.value().size()));
  }

  if (ExpandEnvPathandWow64PathIfFileExists(program_path, &return_program_path))
    return return_program_path;

  // In come cases there are paths with spaces as well as arguments, so we must
  // find where the file path ends and arguments start.
  if (ExtractExecutablePathWithoutArgument(program_path, &return_program_path))
    return return_program_path;

  // Retry with the original input because command-line may incorrectly splits
  // the content.
  program_path = base::FilePath(content);
  if (ExtractExecutablePathWithoutArgument(program_path, &return_program_path))
    return return_program_path;

  // Note: The REG_EXPAND_SZ format is currently not expanded. Paths like
  // %appdata%\\program\\dummy.exe are not expanded.
  return base::FilePath();
}

base::FilePath ExpandEnvPathAndWow64Path(const base::FilePath& path) {
  // Perform environment variables expansion. Replace environment variables like
  // %TEMP% or %APPDATA% with the corresponding absolute path.
  base::FilePath expanded_path;
  if (!ExpandEnvPath(path, &expanded_path))
    expanded_path = path;
  // If the file exists in the current filesystem view, return it.
  if (base::PathExists(expanded_path))
    return expanded_path;
  // Otherwise, perform wow64 path replacement.
  base::FilePath expanded_wow64_path;
  ExpandWow64Path(expanded_path, &expanded_wow64_path);
  return expanded_wow64_path;
}

void RetrievePathInformation(const base::FilePath& expanded_path,
                             internal::FileInformation* file_information) {
  DCHECK(file_information);

  // Set the basic file information.
  file_information->path = SanitizePath(expanded_path);
  file_information->active_file = PathHasActiveExtension(expanded_path);

  base::File::Info file_info;
  if (base::GetFileInfo(expanded_path, &file_info)) {
    file_information->creation_date = TimeFormatDate(file_info.creation_time);
    file_information->last_modified_date =
        TimeFormatDate(file_info.last_modified);
    file_information->size = file_info.size;
  }
}

bool TryToExpandPath(const base::FilePath& file_path,
                     base::FilePath* expanded_path) {
  DCHECK(expanded_path);
  *expanded_path = file_path;
  if (!base::PathExists(*expanded_path))
    ExpandWow64Path(file_path, expanded_path);

  if (!base::PathExists(*expanded_path)) {
    LOG(WARNING)
        << "Unable to retrieve file information on non-existing file: '"
        << SanitizePath(*expanded_path) << "'";
    return false;
  }
  return true;
}

void TruncateLogFileToTail(const base::FilePath& path,
                           int64_t tail_size_bytes) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid() || file.GetLength() < tail_size_bytes)
    return;

  // Read last |tail_size_bytes|.
  int64_t file_offset = file.GetLength() - tail_size_bytes;
  std::vector<char> file_tail(tail_size_bytes);
  int bytes_read = file.Read(file_offset, file_tail.data(), tail_size_bytes);
  file.Close();

  if (bytes_read != tail_size_bytes) {
    // Something went wrong, clean the file.
    base::DeleteFile(path, /*recursive=*/false);
    return;
  }

  // Find first newline character within the tail bytes. That will guarantee
  // not only that the log file with start with a full line, but also that it
  // won't start with a middle byte of a multi-byte UTF8 character.
  auto newline_it = std::find(file_tail.begin(), file_tail.end(), '\n');
  int64_t newline_offset = newline_it - file_tail.begin();

  file.Initialize(path,
                  base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  file.WriteAtCurrentPos(file_tail.data() + newline_offset,
                         tail_size_bytes - newline_offset);
}

}  // namespace chrome_cleaner
