// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UTIL_UTIL_H_
#define CHROME_UPDATER_UTIL_UTIL_H_

#include <cmath>
#include <concepts>
#include <limits>
#include <optional>
#include <ostream>
#include <string>
#include <type_traits>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"
#include "chrome/updater/tag.h"
#include "chrome/updater/updater_scope.h"

class GURL;

namespace base {

class CommandLine;
class FilePath;
class Version;

// Enables insertion of optional `base` types. Must be in the `base` namespace
// for insertion into gTest expectations to work.
template <class T>
inline std::ostream& operator<<(std::ostream& os, const std::optional<T>& opt) {
  if (!opt.has_value()) {
    return os << "std::nullopt";
  }
  return os << opt.value();
}

}  // namespace base

namespace updater {

struct RegistrationRequest;

// Converts an unsigned integral to a signed one. Returns -1 if the value is
// out of the range of the target type.
template <std::unsigned_integral T>
[[nodiscard]] auto ToSignedIntegral(T value) {
  using Result = std::make_signed_t<T>;
  return value <= std::numeric_limits<Result>::max()
             ? static_cast<Result>(value)
             : -1;
}

// Inserts an enum value as the underlying type.
template <typename T>
  requires(std::is_enum_v<T>)
inline std::ostream& operator<<(std::ostream& os, const T& e) {
  return os << base::to_underlying(e);
}

// Returns the versioned install directory under which the program stores its
// executables. For example, on macOS this function may return
// ~/Library/Google/GoogleUpdater/88.0.4293.0 (/Library for system). Does not
// create the directory if it does not exist.
std::optional<base::FilePath> GetVersionedInstallDirectory(
    UpdaterScope scope,
    const base::Version& version);

// Simpler form of GetVersionedInstallDirectory for the currently running
// version of the updater.
std::optional<base::FilePath> GetVersionedInstallDirectory(UpdaterScope scope);

// Returns the base install directory common to all versions of the updater.
// Does not create the directory if it does not exist.
std::optional<base::FilePath> GetInstallDirectory(UpdaterScope scope);

// Returns the base path for discardable caches. Deleting a discardable cache
// between runs of the updater may impair performance, cause a redownload, etc.,
// but otherwise not interfere with overall updater function. Cache contents
// should only be stored in subpaths under this path. Does not create the
// directory if it does not exist.
std::optional<base::FilePath> GetCacheBaseDirectory(UpdaterScope scope);

// Returns the path where CRXes cached for delta updates should be stored,
// common to all versions of the updater. Does not create the directory if it
// does not exist.
std::optional<base::FilePath> GetCrxDiffCacheDirectory(UpdaterScope scope);

#if BUILDFLAG(IS_MAC)
// For example: ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app
std::optional<base::FilePath> GetUpdaterAppBundlePath(UpdaterScope scope);
#endif  // BUILDFLAG(IS_MAC)

// For user installations:
// ~/Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/
//    MacOS/GoogleUpdater
// For system installations:
// /Library/Google/GoogleUpdater/88.0.4293.0/GoogleUpdater.app/Contents/
//    MacOS/GoogleUpdater
std::optional<base::FilePath> GetUpdaterExecutablePath(
    UpdaterScope scope,
    const base::Version& version);

// Simpler form of GetUpdaterExecutablePath for the currently running version
// of the updater.
std::optional<base::FilePath> GetUpdaterExecutablePath(UpdaterScope scope);

// Returns a relative path to the executable from GetVersionedInstallDirectory.
// "GoogleUpdater.app/Contents/MacOS/GoogleUpdater" on macOS.
// "updater.exe" on Win.
base::FilePath GetExecutableRelativePath();

// Returns the path to the crashpad database directory. The directory is not
// created if it does not exist.
std::optional<base::FilePath> GetCrashDatabasePath(UpdaterScope scope);

// Returns the path to the crashpad database, creating it if it does not exist.
std::optional<base::FilePath> EnsureCrashDatabasePath(UpdaterScope scope);

// Contains the parsed values from the tag. The tag is provided as a command
// line argument to the `--install` or the `--handoff` switch.
struct TagParsingResult {
  TagParsingResult();
  TagParsingResult(std::optional<tagging::TagArgs> tag_args,
                   tagging::ErrorCode error);
  ~TagParsingResult();
  TagParsingResult(const TagParsingResult&);
  TagParsingResult& operator=(const TagParsingResult&);
  std::optional<tagging::TagArgs> tag_args;
  tagging::ErrorCode error = tagging::ErrorCode::kSuccess;
};

// These functions return {} if there was no tag at all. An error is set if the
// tag fails to parse.
TagParsingResult GetTagArgsForCommandLine(
    const base::CommandLine& command_line);
TagParsingResult GetTagArgs();

std::optional<tagging::AppArgs> GetAppArgs(const std::string& app_id);

std::string GetDecodedInstallDataFromAppArgs(const std::string& app_id);

std::string GetInstallDataIndexFromAppArgs(const std::string& app_id);

std::optional<base::FilePath> GetLogFilePath(UpdaterScope scope);

// Initializes logging for an executable.
void InitLogging(UpdaterScope updater_scope);

// Returns HTTP user-agent value.
std::string GetUpdaterUserAgent();

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
std::optional<base::CommandLine> CommandLineForLegacyFormat(
    const std::wstring& cmd_string);

// Returns the command line for current process, either in legacy style, or
// in Chromium style.
base::CommandLine GetCommandLineLegacyCompatible();

#endif  // BUILDFLAG(IS_WIN)

// Writes the provided string prefixed with the UTF8 byte order mark to a
// temporary file. The temporary file is created in the specified `directory`.
std::optional<base::FilePath> WriteInstallerDataToTempFile(
    const base::FilePath& directory,
    const std::string& installer_data);

// Creates and starts a thread pool for this process.
void InitializeThreadPool(const char* name);

// Returns whether the user currently running the program is the right user for
// the scope. This can be useful to avoid installing system updaters that are
// owned by non-root accounts, or avoiding the installation of a user level
// updater as root.
bool WrongUser(UpdaterScope scope);

// Returns whether a user has previously accepted a EULA / ToS for at least one
// of the listed apps.
bool EulaAccepted(const std::vector<std::string>& app_ids);

// Imports metadata from legacy updaters.
bool MigrateLegacyUpdaters(
    UpdaterScope scope,
    base::RepeatingCallback<void(const RegistrationRequest&)>
        register_callback);

// Delete everything other than `except` under `except.DirName()`.
[[nodiscard]] bool DeleteExcept(const std::optional<base::FilePath>& except);

// Returns the quotient of dividing two integer numbers (m/n) rounded up.
template <typename T>
  requires(std::integral<T>)
[[nodiscard]] constexpr T CeilingDivide(T m, T n) {
  return std::ceil(static_cast<double>(m) / n);
}

// Returns a value in the [0, 100] range or -1 if the progress could not
// be computed.
[[nodiscard]] int GetDownloadProgress(int64_t downloaded_bytes,
                                      int64_t total_bytes);

// Returns the absolute path to the enterprise companion app executable bundled
// with the updater.
[[nodiscard]] std::optional<base::FilePath>
GetBundledEnterpriseCompanionExecutablePath(UpdaterScope scope);

}  // namespace updater

#endif  // CHROME_UPDATER_UTIL_UTIL_H_
