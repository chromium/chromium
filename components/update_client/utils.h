// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_UTILS_H_
#define COMPONENTS_UPDATE_CLIENT_UTILS_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/update_client/update_client.h"

class GURL;

namespace update_client {

struct CrxComponent;

extern const char kArchAmd64[];
extern const char kArchIntel[];
extern const char kArchArm64[];

// Defines a name-value pair that represents an installer attribute.
// Installer attributes are component-specific metadata, which may be serialized
// in an update check request.
using InstallerAttribute = std::pair<std::string, std::string>;

// Returns true if the |status_code| represents a server error 5xx.
bool IsHttpServerError(int status_code);

// Deletes the file and its directory, if the directory is empty. If the
// parent directory is not empty, the function ignores deleting the directory.
// Returns true if the file and the empty directory are deleted,
// or if the file was deleted and the directory was not empty.
bool DeleteFileAndEmptyParentDirectory(const base::FilePath& filepath);

// Deletes the given directory, if the directory is empty. If the
// directory is not empty, the function ignores deleting the directory.
// Returns true if the directory is not empty or if the directory was empty
// and successfully deleted.
bool DeleteEmptyDirectory(const base::FilePath& filepath);

// Returns the component id of the |component|. The component id is either the
// app_id, if the member is set, or a string value derived from the public
// key hash with a format similar with the format of an extension id.
std::string GetCrxComponentID(const CrxComponent& component);

// Returns a CRX id from a public key hash.
std::string GetCrxIdFromPublicKeyHash(base::span<const uint8_t> pk_hash);

// Returns true if the actual SHA-256 hash of the |filepath| matches the
// |expected_hash|.
bool VerifyFileHash256(const base::FilePath& filepath,
                       const std::string& expected_hash);

// Returns true if the |brand| parameter matches ^[a-zA-Z]{4}?$ .
bool IsValidBrand(const std::string& brand);

// Returns true if the name part of the |attr| parameter matches
// ^[-_a-zA-Z0-9]{1,256}$ and the value part of the |attr| parameter
// matches ^[-.,;+_=$a-zA-Z0-9]{0,256}$ .
bool IsValidInstallerAttribute(const InstallerAttribute& attr);

// Removes the unsecure urls in the |urls| parameter.
void RemoveUnsecureUrls(std::vector<GURL>* urls);

// Adapter function for the old definitions of CrxInstaller::Install until the
// component installer code is migrated to use a Result instead of bool.
CrxInstaller::Result InstallFunctionWrapper(
    base::OnceCallback<bool()> callback);

// Deserializes the CRX manifest. The top level must be a dictionary.
// Returns a base::Value::Dict object of type dictionary on success, or nullopt
// on failure.
std::optional<base::Value::Dict> ReadManifest(
    const base::FilePath& unpack_path);

// Returns a string representation of the processor architecture. Uses
// `base::win::OSInfo::IsWowX86OnARM64` and
// `base::win::OSInfo::IsWowAMD64OnARM64` if available on Windows (more
// accurate).
// If not, or not Windows, falls back to
// `base::SysInfo().OperatingSystemArchitecture`.
std::string GetArchitecture();

// Retries recursively deleting the given path five times using
// `base::DeletePathRecursively`, sleeping for a second between successive
// tries. Returns true if successful, false otherwise. This function is used
// when there is a likelihood that the files in `path` can be locked
// temporarily, such as by antivirus software.
bool RetryDeletePathRecursively(const base::FilePath& path);

// Similar to `RetryDeletePathRecursively`above, but allows specifying the
// number of `tries` and the `seconds_between_tries`.
bool RetryDeletePathRecursivelyCustom(const base::FilePath& path,
                                      size_t tries,
                                      base::TimeDelta seconds_between_tries);

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_UTILS_H_
