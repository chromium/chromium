// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/util.h"

#include <cctype>
#include <string>
#include <vector>

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif  // BUILDFLAG(IS_WIN)

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_branding.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/updater_version.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_LINUX)
#include "chrome/updater/util/linux_util.h"
#elif BUILDFLAG(IS_MAC)
#import "chrome/updater/util/mac_util.h"
#endif

#if BUILDFLAG(IS_POSIX)
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace updater {
namespace {

constexpr int64_t kLogRotateAtSize = 1024 * 1024 * 5;  // 5 MiB.

const char kHexString[] = "0123456789ABCDEF";
inline char IntToHex(int i) {
  DCHECK_GE(i, 0) << i << " not a hex value";
  DCHECK_LE(i, 15) << i << " not a hex value";
  return kHexString[i];
}

// A fast bit-vector map for ascii characters.
//
// Internally stores 256 bits in an array of 8 ints.
// Does quick bit-flicking to lookup needed characters.
struct Charmap {
  bool Contains(unsigned char c) const {
    return ((map[c >> 5] & (1 << (c & 31))) != 0);
  }

  uint32_t map[8] = {};
};

// Everything except alphanumerics and !'()*-._~
// See RFC 2396 for the list of reserved characters.
constexpr Charmap kQueryCharmap = {{0xffffffffL, 0xfc00987dL, 0x78000001L,
                                    0xb8000001L, 0xffffffffL, 0xffffffffL,
                                    0xffffffffL, 0xffffffffL}};

// Given text to escape and a Charmap defining which values to escape,
// return an escaped string.  If use_plus is true, spaces are converted
// to +, otherwise, if spaces are in the charmap, they are converted to
// %20. And if keep_escaped is true, %XX will be kept as it is, otherwise, if
// '%' is in the charmap, it is converted to %25.
std::string Escape(base::StringPiece text,
                   const Charmap& charmap,
                   bool use_plus,
                   bool keep_escaped = false) {
  std::string escaped;
  escaped.reserve(text.length() * 3);
  for (unsigned int i = 0; i < text.length(); ++i) {
    unsigned char c = static_cast<unsigned char>(text[i]);
    if (use_plus && ' ' == c) {
      escaped.push_back('+');
    } else if (keep_escaped && '%' == c && i + 2 < text.length() &&
               base::IsHexDigit(text[i + 1]) && base::IsHexDigit(text[i + 2])) {
      escaped.push_back('%');
    } else if (charmap.Contains(c)) {
      escaped.push_back('%');
      escaped.push_back(IntToHex(c >> 4));
      escaped.push_back(IntToHex(c & 0xf));
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string EscapeQueryParamValue(base::StringPiece text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

}  // namespace

absl::optional<base::FilePath> GetBaseDataDirectory(UpdaterScope scope) {
  absl::optional<base::FilePath> app_data_dir;
#if BUILDFLAG(IS_WIN)
  app_data_dir = GetApplicationDataDirectory(scope);
#elif BUILDFLAG(IS_MAC)
  app_data_dir = GetApplicationSupportDirectory(scope);
#elif BUILDFLAG(IS_LINUX)
  app_data_dir = GetApplicationDataDirectory(scope);
#endif
  if (!app_data_dir) {
    LOG(ERROR) << "Can't retrieve app data directory.";
    return absl::nullopt;
  }

  const auto product_data_dir =
      app_data_dir->AppendASCII(COMPANY_SHORTNAME_STRING)
          .AppendASCII(PRODUCT_FULLNAME_STRING);
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(product_data_dir, &error)) {
    LOG(WARNING) << "Can't create base directory: " << product_data_dir << ": "
                 << error;
    return absl::nullopt;
  }
  return product_data_dir;
}

absl::optional<base::FilePath> GetVersionedDataDirectory(UpdaterScope scope) {
  const absl::optional<base::FilePath> product_dir =
      GetBaseDataDirectory(scope);
  if (!product_dir) {
    LOG(ERROR) << "Failed to get the base directory.";
    return absl::nullopt;
  }

  const auto versioned_dir = product_dir->AppendASCII(kUpdaterVersion);
  if (!base::CreateDirectory(versioned_dir)) {
    LOG(ERROR) << "Can't create versioned directory.";
    return absl::nullopt;
  }

  return versioned_dir;
}

absl::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope,
    const base::Version& version) {
  const absl::optional<base::FilePath> path = GetBaseInstallDirectory(scope);
  if (!path)
    return absl::nullopt;
  return path->AppendASCII(version.GetString());
}

absl::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope) {
  return GetVersionedInstallDirectory(scope, base::Version(kUpdaterVersion));
}

absl::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope) {
  absl::optional<base::FilePath> path = GetVersionedInstallDirectory(scope);
  if (!path) {
    return absl::nullopt;
  }
  return path->Append(GetExecutableRelativePath());
}

TagParsingResult::TagParsingResult() = default;
TagParsingResult::TagParsingResult(absl::optional<tagging::TagArgs> tag_args,
                                   tagging::ErrorCode error)
    : tag_args(tag_args), error(error) {}
TagParsingResult::~TagParsingResult() = default;
TagParsingResult::TagParsingResult(const TagParsingResult&) = default;
TagParsingResult& TagParsingResult::operator=(const TagParsingResult&) =
    default;

TagParsingResult GetTagArgsForCommandLine(
    const base::CommandLine& command_line) {
  std::string tag = command_line.HasSwitch(kTagSwitch)
                        ? command_line.GetSwitchValueASCII(kTagSwitch)
                        : command_line.GetSwitchValueASCII(kHandoffSwitch);
  if (tag.empty())
    return {};

  tagging::TagArgs tag_args;
  const tagging::ErrorCode error = tagging::Parse(
      tag, command_line.GetSwitchValueASCII(kAppArgsSwitch), &tag_args);
  VLOG_IF(1, error != tagging::ErrorCode::kSuccess)
      << "Tag parsing returned " << error << ".";
  return {tag_args, error};
}

TagParsingResult GetTagArgs() {
  return GetTagArgsForCommandLine(*base::CommandLine::ForCurrentProcess());
}

absl::optional<tagging::AppArgs> GetAppArgsForCommandLine(
    const base::CommandLine& command_line,
    const std::string& app_id) {
  const absl::optional<tagging::TagArgs> tag_args =
      GetTagArgsForCommandLine(command_line).tag_args;
  if (!tag_args || tag_args->apps.empty())
    return absl::nullopt;

  const std::vector<tagging::AppArgs>& apps_args = tag_args->apps;
  std::vector<tagging::AppArgs>::const_iterator it = base::ranges::find_if(
      apps_args, [&app_id](const tagging::AppArgs& app_args) {
        return base::EqualsCaseInsensitiveASCII(app_args.app_id, app_id);
      });
  return it != std::end(apps_args) ? absl::optional<tagging::AppArgs>(*it)
                                   : absl::nullopt;
}

absl::optional<tagging::AppArgs> GetAppArgs(const std::string& app_id) {
  return GetAppArgsForCommandLine(*base::CommandLine::ForCurrentProcess(),
                                  app_id);
}

std::string GetDecodedInstallDataFromAppArgsForCommandLine(
    const base::CommandLine& command_line,
    const std::string& app_id) {
  const absl::optional<tagging::AppArgs> app_args =
      GetAppArgsForCommandLine(command_line, app_id);
  if (!app_args)
    return std::string();

  std::string decoded_installer_data;
  const bool result = base::UnescapeBinaryURLComponentSafe(
      app_args->encoded_installer_data,
      /*fail_on_path_separators=*/false, &decoded_installer_data);
  VLOG_IF(1, !result) << "Failed to decode encoded installer data: ["
                      << app_args->encoded_installer_data << "]";

  // `decoded_installer_data` is set to empty if
  // `UnescapeBinaryURLComponentSafe` fails.
  return decoded_installer_data;
}

std::string GetDecodedInstallDataFromAppArgs(const std::string& app_id) {
  return GetDecodedInstallDataFromAppArgsForCommandLine(
      *base::CommandLine::ForCurrentProcess(), app_id);
}

std::string GetInstallDataIndexFromAppArgsForCommandLine(
    const base::CommandLine& command_line,
    const std::string& app_id) {
  const absl::optional<tagging::AppArgs> app_args =
      GetAppArgsForCommandLine(command_line, app_id);
  return app_args ? app_args->install_data_index : std::string();
}

std::string GetInstallDataIndexFromAppArgs(const std::string& app_id) {
  return GetInstallDataIndexFromAppArgsForCommandLine(
      *base::CommandLine::ForCurrentProcess(), app_id);
}

base::CommandLine MakeElevated(base::CommandLine command_line) {
#if BUILDFLAG(IS_MAC)
  command_line.PrependWrapper("/usr/bin/sudo");
#endif
  return command_line;
}

// The log file is created in DIR_LOCAL_APP_DATA or DIR_ROAMING_APP_DATA.
absl::optional<base::FilePath> GetLogFilePath(UpdaterScope scope) {
  const absl::optional<base::FilePath> log_dir = GetBaseDataDirectory(scope);
  if (log_dir) {
    return log_dir->Append(FILE_PATH_LITERAL("updater.log"));
  }
  return absl::nullopt;
}

void InitLogging(UpdaterScope updater_scope) {
  absl::optional<base::FilePath> log_file = GetLogFilePath(updater_scope);
  if (!log_file) {
    LOG(ERROR) << "Error getting base dir.";
    return;
  }
  // Rotate log if needed.
  int64_t size = 0;
  if (base::GetFileSize(*log_file, &size) && size >= kLogRotateAtSize) {
    base::ReplaceFile(
        *log_file, log_file->AddExtension(FILE_PATH_LITERAL(".old")), nullptr);
  }
  logging::LoggingSettings settings;
  settings.log_file_path = log_file->value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);
  VLOG(1) << "Log initialized for " <<
      []() {
        base::FilePath file_exe;
        return base::PathService::Get(base::FILE_EXE, &file_exe)
                   ? file_exe
                   : base::FilePath();
      }() << " -> "
          << settings.log_file_path;
}

// This function and the helper functions are copied from net/base/url_util.cc
// to avoid the dependency on //net.
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty())
    query += "&";

  query += (EscapeQueryParamValue(name, true) + "=" +
            EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

#if BUILDFLAG(IS_POSIX)

bool PathOwnedByUser(const base::FilePath& path) {
  struct passwd* result = nullptr;
  struct passwd user_info = {};
  char pwbuf[2048] = {};
  const uid_t user_uid = geteuid();

  const int error =
      getpwuid_r(user_uid, &user_info, pwbuf, sizeof(pwbuf), &result);

  if (error) {
    VLOG(1) << "Failed to get user info.";
    return true;
  }

  if (result == nullptr) {
    VLOG(1) << "No entry for user.";
    return true;
  }

  base::stat_wrapper_t stat_info = {};
  if (base::File::Lstat(path.value().c_str(), &stat_info) != 0) {
    DPLOG(ERROR) << "Failed to get information on path " << path.value();
    return false;
  }

  if (S_ISLNK(stat_info.st_mode)) {
    DLOG(ERROR) << "Path " << path.value() << " is a symbolic link.";
    return false;
  }

  if (stat_info.st_uid != user_uid) {
    DLOG(ERROR) << "Path " << path.value() << " is owned by the wrong user.";
    return false;
  }

  return true;
}

#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)

std::wstring GetTaskNamePrefix(UpdaterScope scope) {
  std::wstring task_name = GetTaskDisplayName(scope);
  task_name.erase(base::ranges::remove_if(task_name, isspace), task_name.end());
  return task_name;
}

std::wstring GetTaskDisplayName(UpdaterScope scope) {
  return base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" Task ",
                       IsSystemInstall(scope) ? L"System " : L"User ",
                       kUpdaterVersionUtf16});
}

base::CommandLine GetCommandLineLegacyCompatible() {
  absl::optional<base::CommandLine> cmd_line =
      CommandLineForLegacyFormat(::GetCommandLine());
  return cmd_line ? *cmd_line : *base::CommandLine::ForCurrentProcess();
}

#else  // BUILDFLAG(IS_WIN)

base::CommandLine GetCommandLineLegacyCompatible() {
  return *base::CommandLine::ForCurrentProcess();
}

#endif  // BUILDFLAG(IS_WIN)

absl::optional<base::FilePath> WriteInstallerDataToTempFile(
    const base::FilePath& directory,
    const std::string& installer_data) {
  VLOG(2) << __func__ << ": " << directory << ": " << installer_data;

  if (!base::DirectoryExists(directory))
    return absl::nullopt;

  if (installer_data.empty())
    return absl::nullopt;

  base::FilePath path;
  base::File file = base::CreateAndOpenTemporaryFileInDir(directory, &path);
  if (!file.IsValid())
    return absl::nullopt;

  const std::string installer_data_utf8_bom =
      base::StrCat({kUTF8BOM, installer_data});
  if (file.Write(0, installer_data_utf8_bom.c_str(),
                 installer_data_utf8_bom.length()) == -1) {
    VLOG(2) << __func__ << " file.Write failed";
    return absl::nullopt;
  }

  return path;
}

void InitializeThreadPool(const char* name) {
  base::ThreadPoolInstance::Create(name);

  // Reuses the logic in base::ThreadPoolInstance::StartWithDefaultParams.
  const size_t max_num_foreground_threads =
      static_cast<size_t>(std::max(3, base::SysInfo::NumberOfProcessors() - 1));
  base::ThreadPoolInstance::InitParams init_params(max_num_foreground_threads);
#if BUILDFLAG(IS_WIN)
  init_params.common_thread_pool_environment = base::ThreadPoolInstance::
      InitParams::CommonThreadPoolEnvironment::COM_MTA;
#endif
  base::ThreadPoolInstance::Get()->Start(init_params);
}

}  // namespace updater
