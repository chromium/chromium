// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_MANIFEST_UTIL_H_
#define CHROME_UPDATER_WIN_MANIFEST_UTIL_H_

#include <string>

#include "components/update_client/protocol_parser.h"

namespace base {
class FilePath;
}

namespace updater {

// Parses the offline manifest file and extracts the app install command line.
//
// The function looks for the manifest file "OfflineManifest.gup" inside the
// offline directory, and falls back to "<app_id>.gup" in the same directory if
// needed.
//
// `offline_dir_guid`: the offline directory is specified on the command line as
// a relative path in the format "/offlinedir {GUID}", where `{GUID}` is the
// `offline_dir_guid` parameter.
// * The actual offline directory is at `{CURRENT_PROCESS_DIR}\Offline\{GUID}`.
// * The offline manifest is at
// `{CURRENT_PROCESS_DIR}\Offline\{GUID}\OfflineManifest.gup`.
// * The installer is at
// `{CURRENT_PROCESS_DIR}\Offline\{GUID}\{app_id}\installer.exe`.
//   * `installer.exe` may not correspond exactly to the value of the manifest's
//   `run` attribute, so the code picks the first file it finds in the
//   directory if that is the case.
//
// The manifest file contains the update check response in XML format.
// See https://github.com/google/omaha/blob/master/doc/ServerProtocol.md for
// protocol details.
//
// The function extracts the values from the manifest using a best-effort
// approach. If matching values are found, then:
//   `results`: contains the protocol parser results.
//   `installer_version`: contains the version of the app installer.
//   `installer_path`: contains the full path to the app installer.
//   `install_args`: the command line arguments for the app installer.
//   `install_data`: the text value for the key `install_data_index` if such
//                   key/value pair exists in <data> element. During
//                   installation, the text will be serialized to a file and
//                   passed to the app installer.
void ReadInstallCommandFromManifest(
    const std::wstring& offline_dir_guid,
    const std::string& app_id,
    const std::string& install_data_index,
    update_client::ProtocolParser::Results& results,
    std::string& installer_version,
    base::FilePath& installer_path,
    std::string& install_args,
    std::string& install_data);

// Returns `true` if:
//* `arch` is empty, or
// * `arch` matches the current architecture, or
// * `arch` is supported on the machine, as determined by
// `::IsWow64GuestMachineSupported()`.
//   * If `::IsWow64GuestMachineSupported()` is not available, returns `true`
//     if `arch` is x86.
bool IsArchitectureSupported(const std::string& arch,
                             const std::string& current_architecture);

// Returns `true` if `platform` is empty or equals "win".
bool IsPlatformCompatible(const std::string& platform);

// Checks if the current architecture is compatible with the entries in
// `arch_list`. `arch_list` can be a single entry, or multiple entries separated
// with `,`. Entries prefixed with `-` (negative entries) indicate
// non-compatible hosts. Non-prefixed entries indicate compatible guests.
//
// Returns `true` if:
// * `arch_list` is empty, or
// * none of the negative entries within `arch_list` match the current host
//   architecture exactly, and there are no non-negative entries, or
// * one of the non-negative entries within `arch_list` matches the current
//   architecture, or is compatible with the current architecture (i.e., it is a
//   compatible guest for the current host) as determined by
//   `::IsWow64GuestMachineSupported()`.
//   * If `::IsWow64GuestMachineSupported()` is not available, returns `true`
//     if `arch` is x86.
//
// Examples:
// * `arch_list` == "x86": returns `true` if run on all systems, because the
//   Updater is x86, and is running the logic to determine compatibility).
// * `arch_list` == "x64": returns `true` if run on x64 or many arm64 systems.
// * `arch_list` == "x86,x64,-arm64": returns `false` if the underlying host is
// arm64.
// * `arch_list` == "-arm64": returns `false` if the underlying host is arm64.
bool IsArchitectureCompatible(const std::string& arch_list,
                              const std::string& current_architecture);

// Returns `true` if `min_os_version` is valid and is less than or equal to the
// current OS version in the format "major.minor.build.patch".
//
// A valid `min_os_version` is in the format `major.minor.build.patch`. The
// `major`, `minor` and `build` are the values returned by `::GetVersionEx`. The
// `patch` is the `UBR` value under the registry path
// `HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion`.
//
// The `build` and the `patch` components may be omitted if all that is needed
// is a minimum `major.minor` version. For example, `6.0` will match all OS
// versions that are at or above that version, regardless of `build` and `patch`
// numbers.
bool IsOSVersionCompatible(const std::string& min_os_version);

// Returns `true` if the platform, architecture, and OS within the parser
// `results` are all compatible with the current OS.
bool IsOsSupported(const update_client::ProtocolParser::Results& results);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_MANIFEST_UTIL_H_
