// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/updater/util/util.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if BUILDFLAG(IS_WIN)
#include <initguid.h>
#include <windows.h>

#include "base/logging_win.h"
#endif  // BUILDFLAG(IS_WIN)

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

constexpr int64_t kLogRotateAtSize = 1024 * 1024;  // 1 MiB.

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
std::string Escape(std::string_view text,
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
      base::AppendHexEncodedByte(c, escaped);
    } else {
      escaped.push_back(c);
    }
  }
  return escaped;
}

std::string EscapeQueryParamValue(std::string_view text, bool use_plus) {
  return Escape(text, kQueryCharmap, use_plus);
}

}  // namespace

std::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope,
    const base::Version& version) {
  const std::optional<base::FilePath> path = GetInstallDirectory(scope);
  if (!path) {
    return std::nullopt;
  }
  return path->AppendASCII(version.GetString());
}

std::optional<base::FilePath> GetVersionedInstallDirectory(UpdaterScope scope) {
  return GetVersionedInstallDirectory(scope, base::Version(kUpdaterVersion));
}

std::optional<base::FilePath> GetUpdaterExecutablePath(
    UpdaterScope scope,
    const base::Version& version) {
  std::optional<base::FilePath> path =
      GetVersionedInstallDirectory(scope, version);
  if (!path) {
    return std::nullopt;
  }
  return path->Append(GetExecutableRelativePath());
}

#if !BUILDFLAG(IS_MAC)
std::optional<base::FilePath> GetCacheBaseDirectory(UpdaterScope scope) {
  return GetInstallDirectory(scope);
}
#endif

std::optional<base::FilePath> GetCrxDiffCacheDirectory(UpdaterScope scope) {
  const std::optional<base::FilePath> cache_path(GetCacheBaseDirectory(scope));
  if (!cache_path) {
    return std::nullopt;
  }
  return std::optional<base::FilePath>(cache_path->AppendASCII("crx_cache"));
}

std::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope) {
  return GetUpdaterExecutablePath(scope, base::Version(kUpdaterVersion));
}

std::optional<base::FilePath> GetCrashDatabasePath(UpdaterScope scope) {
  const std::optional<base::FilePath> path(GetVersionedInstallDirectory(scope));
  return path ? std::optional<base::FilePath>(path->AppendASCII("Crashpad"))
              : std::nullopt;
}

std::optional<base::FilePath> EnsureCrashDatabasePath(UpdaterScope scope) {
  const std::optional<base::FilePath> database_path(
      GetCrashDatabasePath(scope));
  return database_path && base::CreateDirectory(*database_path) ? database_path
                                                                : std::nullopt;
}

TagParsingResult::TagParsingResult() = default;
TagParsingResult::TagParsingResult(std::optional<tagging::TagArgs> tag_args,
                                   tagging::ErrorCode error)
    : tag_args(tag_args), error(error) {}
TagParsingResult::~TagParsingResult() = default;
TagParsingResult::TagParsingResult(const TagParsingResult&) = default;
TagParsingResult& TagParsingResult::operator=(const TagParsingResult&) =
    default;

TagParsingResult GetTagArgsForCommandLine(
    const base::CommandLine& command_line) {
  std::string tag = command_line.HasSwitch(kInstallSwitch)
                        ? command_line.GetSwitchValueASCII(kInstallSwitch)
                        : command_line.GetSwitchValueASCII(kHandoffSwitch);
  if (tag.empty()) {
    return {};
  }

  tagging::TagArgs tag_args;
  const tagging::ErrorCode error = tagging::Parse(
      tag, command_line.GetSwitchValueASCII(kAppArgsSwitch), tag_args);
  VLOG_IF(1, error != tagging::ErrorCode::kSuccess)
      << "Tag parsing returned " << error << ".";
  return {tag_args, error};
}

TagParsingResult GetTagArgs() {
  return GetTagArgsForCommandLine(*base::CommandLine::ForCurrentProcess());
}

std::optional<tagging::AppArgs> GetAppArgs(const std::string& app_id) {
  const std::optional<tagging::TagArgs> tag_args = GetTagArgs().tag_args;
  if (!tag_args || tag_args->apps.empty()) {
    return std::nullopt;
  }

  const std::vector<tagging::AppArgs>& apps_args = tag_args->apps;
  std::vector<tagging::AppArgs>::const_iterator it = base::ranges::find_if(
      apps_args, [&app_id](const tagging::AppArgs& app_args) {
        return base::EqualsCaseInsensitiveASCII(app_args.app_id, app_id);
      });
  return it != std::end(apps_args) ? std::optional<tagging::AppArgs>(*it)
                                   : std::nullopt;
}

std::string GetDecodedInstallDataFromAppArgs(const std::string& app_id) {
  const std::optional<tagging::AppArgs> app_args = GetAppArgs(app_id);
  if (!app_args) {
    return std::string();
  }

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

std::string GetInstallDataIndexFromAppArgs(const std::string& app_id) {
  const std::optional<tagging::AppArgs> app_args = GetAppArgs(app_id);
  return app_args ? app_args->install_data_index : std::string();
}

std::optional<base::FilePath> GetLogFilePath(UpdaterScope scope) {
  const std::optional<base::FilePath> log_dir = GetInstallDirectory(scope);
  if (log_dir) {
    return log_dir->Append(FILE_PATH_LITERAL("updater.log"));
  }
  return std::nullopt;
}

void InitLogging(UpdaterScope updater_scope) {
  std::optional<base::FilePath> log_file = GetLogFilePath(updater_scope);
  if (!log_file) {
    LOG(ERROR) << "Error getting base dir.";
    return;
  }
  base::CreateDirectory(log_file->DirName());
  // Rotate log if needed.
  int64_t size = 0;
  if (base::GetFileSize(*log_file, &size) && size >= kLogRotateAtSize) {
    base::ReplaceFile(
        *log_file, log_file->AddExtension(FILE_PATH_LITERAL(".old")), nullptr);
  }
  logging::LoggingSettings settings;
  settings.log_file_path = log_file->value().c_str();
  settings.logging_dest = logging::LOG_TO_ALL;
#if BUILDFLAG(IS_WIN)
  settings.logging_dest &= ~logging::LOG_TO_SYSTEM_DEBUG_LOG;
#endif  // BUILDFLAG(IS_WIN)
  logging::InitLogging(settings);
  logging::SetLogItems(/*enable_process_id=*/true,
                       /*enable_thread_id=*/true,
                       /*enable_timestamp=*/true,
                       /*enable_tickcount=*/false);

#if BUILDFLAG(IS_WIN)
  // Enable Event Tracing for Windows.
  // {4D7D9607-78B6-4583-A188-2136AB85F5F1}
  constexpr GUID kUpdaterETWProviderName = {
      0x4d7d9607,
      0x78b6,
      0x4583,
      {0xa1, 0x88, 0x21, 0x36, 0xab, 0x85, 0xf5, 0xf1}};
  logging::LogEventProvider::Initialize(kUpdaterETWProviderName);
#endif
}

std::string GetUpdaterUserAgent() {
  return base::StrCat({PRODUCT_FULLNAME_STRING, " ", kUpdaterVersion});
}

// This function and the helper functions are copied from net/base/url_util.cc
// to avoid the dependency on //net.
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value) {
  std::string query(url.query());

  if (!query.empty()) {
    query += "&";
  }

  query += (EscapeQueryParamValue(name, true) + "=" +
            EscapeQueryParamValue(value, true));
  GURL::Replacements replacements;
  replacements.SetQueryStr(query);
  return url.ReplaceComponents(replacements);
}

#if BUILDFLAG(IS_WIN)

std::wstring GetTaskNamePrefix(UpdaterScope scope) {
  std::wstring task_name = GetTaskDisplayName(scope);
  std::erase_if(task_name, base::IsAsciiWhitespace<wchar_t>);
  return task_name;
}

std::wstring GetTaskDisplayName(UpdaterScope scope) {
  return base::StrCat({base::ASCIIToWide(PRODUCT_FULLNAME_STRING), L" Task ",
                       IsSystemInstall(scope) ? L"System " : L"User ",
                       kUpdaterVersionUtf16});
}

base::CommandLine GetCommandLineLegacyCompatible() {
  const std::wstring cmd_string = ::GetCommandLine();
  std::optional<base::CommandLine> cmd_line =
      CommandLineForLegacyFormat(cmd_string);
  return cmd_line ? *cmd_line : base::CommandLine::FromString(cmd_string);
}

#endif  // BUILDFLAG(IS_WIN)

std::optional<base::FilePath> WriteInstallerDataToTempFile(
    const base::FilePath& directory,
    const std::string& installer_data) {
  VLOG(2) << __func__ << ": " << directory << ": " << installer_data;

  if (!base::DirectoryExists(directory)) {
    return std::nullopt;
  }

  if (installer_data.empty()) {
    return std::nullopt;
  }

  base::FilePath path;
  base::File file = base::CreateAndOpenTemporaryFileInDir(directory, &path);
  if (!file.IsValid()) {
    return std::nullopt;
  }

  const std::string installer_data_utf8_bom =
      base::StrCat({kUTF8BOM, installer_data});
  if (file.Write(0, installer_data_utf8_bom.c_str(),
                 installer_data_utf8_bom.length()) == -1) {
    VLOG(2) << __func__ << " file.Write failed";
    return std::nullopt;
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

bool DeleteExcept(const std::optional<base::FilePath>& except) {
  if (!except) {
    return false;
  }
  bool delete_success = true;
  base::FileEnumerator(
      except->DirName(), false,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES)
      .ForEach([&](const base::FilePath& item) {
        if (item != *except) {
          VLOG(2) << "DeleteExcept deleting: " << item;
          if (!base::DeletePathRecursively(item)) {
            VPLOG(1) << "DeleteExcept failed to delete: " << item;
            delete_success = false;
          }
        }
      });
  return delete_success;
}

int GetDownloadProgress(int64_t downloaded_bytes, int64_t total_bytes) {
  if (downloaded_bytes == -1 || total_bytes == -1 || total_bytes == 0) {
    return -1;
  }
  return 100 * std::clamp(static_cast<double>(downloaded_bytes) / total_bytes,
                          0.0, 1.0);
}

}  // namespace updater
