// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_SETUP_INSTALL_COMPONENT_H_
#define CHROME_INSTALLER_SETUP_INSTALL_COMPONENT_H_

#include <stdint.h>

#include <array>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/function_ref.h"
#include "base/version.h"
#include "chrome/installer/setup/component_interface.h"
#include "chrome/installer/util/util_constants.h"
#include "crypto/hash.h"

namespace base {
class CommandLine;
}  // namespace base

namespace crx_file {
enum class VerifierFormat;
}

namespace installer {

class InstallerState;

// Installs a Chrome component from an inner CRX to the Chrome application
// directory (system-level or user-level depending on `installer_state`).
//
// `source_file`: Path to the inner CRX component file to install.
// `installer_state`: State encapsulating target paths and installation level.
// `command_line`: The installer's command line, from which component-specific
// options are read.
// Returns InstallStatus indicating the result of the installation.
InstallStatus InstallComponent(const base::FilePath& source_file,
                               const InstallerState& installer_state,
                               const base::CommandLine& command_line);

// Factory function type that maps a component public key SHA256 digest to its
// ComponentInterface instance. Returns nullptr if the component is unsupported.
using ComponentFactory = base::FunctionRef<std::unique_ptr<ComponentInterface>(
    const std::array<uint8_t, crypto::hash::kSha256Size>&)>;

// Test-only entry point that allows specifying a custom component factory
// (overriding the supported components) and verifier format.
InstallStatus InstallComponentForTesting(
    const base::FilePath& source_file,
    const InstallerState& installer_state,
    ComponentFactory component_factory,
    crx_file::VerifierFormat verifier_format);

// Returns the parsed component version if the name of `version_dir` is a
// version in canonical form (as produced by `base::Version::GetString()`) and
// it contains a `manifest.json` file, or an invalid version otherwise.
base::Version GetComponentVersion(const base::FilePath& version_dir);

// Returns the highest valid component version installed in `component_root`,
// or an invalid version if no valid version directories exist.
base::Version FindHighestComponentVersion(const base::FilePath& component_root);

// Deletes subdirectories under `component_root` that are either corrupted
// (invalid or missing `manifest.json`) or represent versions strictly older
// than `keep_version`.
void DeleteInvalidComponentDirectories(const base::FilePath& component_root,
                                       const base::Version& keep_version);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_INSTALL_COMPONENT_H_
