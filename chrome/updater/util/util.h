// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_UTIL_H_
#define CHROME_UPDATER_UTIL_UTIL_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_scope.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

// Externally-defined printers for base types.
namespace base {

class CommandLine;
class Version;

template <class T>
std::ostream& operator<<(std::ostream& os, const absl::optional<T>& opt) {
  if (opt.has_value()) {
    return os << opt.value();
  } else {
    return os << "absl::nullopt";
  }
}

}  // namespace base

namespace updater {

// This template function enables logging enum value as the underlying type.
template <typename T>
std::ostream& operator<<(
    typename std::enable_if<std::is_enum<T>::value, std::ostream>::type& stream,
    const T& e) {
  return stream << base::to_underlying(e);
}

namespace tagging {
struct TagArgs;
}

enum class UpdaterScope;

// Returns the versioned install directory under which the program stores its
// executables. For example, on macOS this function may return
// ~/Library/Google/GoogleUpdater/88.0.4293.0 (/Library for system). Does not
// create the directory if it does not exist.
absl::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope,
    const base::Version& version);

// Simpler form of GetVersionedInstallDirectory for the currently running
// version of the updater.
absl::optional<base::FilePath> GetVersionedInstallDirectory(UpdaterScope scope);

// Returns the base install directory common to all versions of the updater.
// Does not create the directory if it does not exist.
absl::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope);

#if BUILDFLAG(IS_MAC)
// For example: ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app
absl::optional<base::FilePath> GetUpdaterAppBundlePath(UpdaterScope scope);
#endif  // BUILDFLAG(IS_MAC)

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/
//    MacOS/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/
//    MacOS/GoogleUpdater
absl::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope);

// Returns a relative path to the executable from GetVersionedInstallDirectory.
// "GoogleUpdater.app/Contents/MacOS/GoogleUpdater" on macOS.
// "updater.exe" on Win.
base::FilePath GetExecutableRelativePath();

// Returns the path to the crashpad database directory. The directory is not
// created if it does not exist.
absl::optional<base::FilePath> GetCrashDatabasePath(UpdaterScope scope);

// Returns the path to the crashpad database, creating it if it does not exist.
absl::optional<base::FilePath> EnsureCrashDatabasePath(UpdaterScope scope);

// Return the parsed values from --tag command line argument. The functions
// return {} if there was no tag at all. An error is set if the tag fails to
// parse.
struct TagParsingResult {
  TagParsingResult();
  TagParsingResult(absl::optional<tagging::TagArgs> tag_args,
                   tagging::ErrorCode error);
  ~TagParsingResult();
  TagParsingResult(const TagParsingResult&);
  TagParsingResult& operator=(const TagParsingResult&);
  absl::optional<tagging::TagArgs> tag_args;
  tagging::ErrorCode error = tagging::ErrorCode::kSuccess;
};

TagParsingResult GetTagArgsForCommandLine(
    const base::CommandLine& command_line);
TagParsingResult GetTagArgs();

absl::optional<tagging::AppArgs> GetAppArgs(const std::string& app_id);

std::string GetDecodedInstallDataFromAppArgs(const std::string& app_id);

std::string GetInstallDataIndexFromAppArgs(const std::string& app_id);

absl::optional<base::FilePath> GetLogFilePath(UpdaterScope scope);

// Initializes logging for an executable.
void InitLogging(UpdaterScope updater_scope);

// Returns a new GURL by appending the given query parameter name and the
// value. Unsafe characters in the name and the value are escaped like
// %XX%XX. The original query component is preserved if it's present.
//
// Examples:
//
// AppendQueryParameter(GURL("http://example.com"), "name", "value").spec()
// => "http://example.com?name=value"
// AppendQueryParameter(GURL("http://example.com?x=y"), "name", "value").spec()
// => "http://example.com?x=y&name=value"
GURL AppendQueryParameter(const GURL& url,
                          const std::string& name,
                          const std::string& value);

#if BUILDFLAG(IS_MAC)
// Uses the builtin unzip utility within macOS /usr/bin/unzip to unzip instead
// of using the configurator's UnzipperFactory. The UnzipperFactory utilizes the
// //third_party/zlib/google, which has a bug that does not preserve the
// permissions when it extracts the contents. For updates via zip or
// differentials, use UnzipWithExe.
bool UnzipWithExe(const base::FilePath& src_path,
                  const base::FilePath& dest_path);

absl::optional<base::FilePath> GetKeystoneFolderPath(UpdaterScope scope);

// Read the file at path to confirm that the file at the path has the same
// permissions as the given permissions mask.
bool ConfirmFilePermissions(const base::FilePath& root_path,
                            int kPermissionsMask);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

// Returns the versioned task name prefix in the following format:
// "{ProductName}Task{System/User}{UpdaterVersion}".
// For instance: "ChromiumUpdaterTaskSystem92.0.0.1".
std::wstring GetTaskNamePrefix(UpdaterScope scope);

// Returns the versioned task display name in the following format:
// "{ProductName} Task {System/User} {UpdaterVersion}".
// For instance: "ChromiumUpdater Task System 92.0.0.1".
std::wstring GetTaskDisplayName(UpdaterScope scope);

// Parses the command line string in legacy format into `base::CommandLine`.
// The string must be in format like:
//   program.exe /switch1 value1 /switch2 /switch3 value3
// Returns empty if a Chromium style switch is found.
absl::optional<base::CommandLine> CommandLineForLegacyFormat(
    const std::wstring& cmd_string);

// Returns the command line for current process, either in legacy style, or
// in Chromium style.
base::CommandLine GetCommandLineLegacyCompatible();

#endif  // BUILDFLAG(IS_WIN)

// Writes the provided string prefixed with the UTF8 byte order mark to a
// temporary file. The temporary file is created in the specified `directory`.
absl::optional<base::FilePath> WriteInstallerDataToTempFile(
    const base::FilePath& directory,
    const std::string& installer_data);

// Creates and starts a thread pool for this process.
void InitializeThreadPool(const char* name);

// Returns whether the user currently running the program is the right user for
// the scope. This can be useful to avoid installing system updaters that are
// owned by non-root accounts, or avoiding the installation of a user level
// updater as root.
bool WrongUser(UpdaterScope scope);

// Delete everything other than `except` under `except.DirName()`.
[[nodiscard]] bool DeleteExcept(const absl::optional<base::FilePath>& except);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_UTIL_H_
