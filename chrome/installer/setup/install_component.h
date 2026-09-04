// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_COMPONENT_H_
#define CHROME_INSTALLER_SETUP_INSTALL_COMPONENT_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/version.h"
#include "chrome/installer/util/util_constants.h"
#include "components/crx_file/crx_verifier.h"

namespace base {
class FilePath;
}  // namespace base

namespace installer {

class InstallerState;

// Configuration for a supported component, linking its expected manifest name
// and pinned public key SHA256 hash.
struct ComponentConfig {
  std::string_view manifest_name;
  base::raw_span<const uint8_t> public_key_sha256;
};

// Installs a Chrome component from an inner CRX to the Chrome application
// directory under its CRX ID (system-level or user-level depending on
// `installer_state`).
//
// `source_file`: Path to the inner CRX component file to install.
// `installer_state`: State encapsulating target paths and installation level.
// Returns InstallStatus indicating the result of the installation.
InstallStatus InstallComponent(const base::FilePath& source_file,
                               const InstallerState& installer_state);

// Test-only entry point that allows specifying custom supported components
// (overriding the developer keys) and verifier format.
InstallStatus InstallComponentForTesting(
    const base::FilePath& source_file,
    const InstallerState& installer_state,
    base::span<const ComponentConfig> supported_components,
    crx_file::VerifierFormat verifier_format);

// Returns the parsed component version if `version_dir` has a valid version
// directory name and contains a valid `manifest.json` file.
std::optional<base::Version> GetComponentVersion(
    const base::FilePath& version_dir);

// Returns the highest valid component version installed in `component_root`,
// or `std::nullopt` if no valid version directories exist.
std::optional<base::Version> FindHighestComponentVersion(
    const base::FilePath& component_root);

// Deletes subdirectories under `component_root` that are either corrupted
// (invalid or missing `manifest.json`) or represent versions strictly older
// than `keep_version`.
void DeleteInvalidComponentDirectories(const base::FilePath& component_root,
                                       const base::Version& keep_version);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_COMPONENT_H_
