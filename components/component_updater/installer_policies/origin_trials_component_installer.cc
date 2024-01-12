// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/origin_trials_component_installer.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/values.h"
#include "components/component_updater/component_updater_paths.h"

// The client-side configuration for the origin trial framework can be
// overridden by an installed component named 'OriginTrials' (extension id
// llkgjffcdpffmhiakmfcdcblohccpfmo. This component currently consists of just a
// manifest.json file, which can contain a custom key named 'origin-trials'. The
// value of this key is a dictionary:
//
// {
//   "public-key": "<base64-encoding of replacement public key>",
//   "disabled-features": [<list of features to disable>],
//   "disabled-tokens":
//   {
//     "signatures": [<list of token signatures to disable, base64-encoded>]
//   }
// }
//
// If the component is not present in the user data directory, the default
// configuration will be used.

namespace component_updater {

namespace {

static const char kManifestOriginTrialsKey[] = "origin-trials";

// Extension id is llkgjffcdpffmhiakmfcdcblohccpfmo
const uint8_t kOriginTrialSha2Hash[] = {
    0xbb, 0xa6, 0x95, 0x52, 0x3f, 0x55, 0xc7, 0x80, 0xac, 0x52, 0x32,
    0x1b, 0xe7, 0x22, 0xf5, 0xce, 0x6a, 0xfd, 0x9c, 0x9e, 0xa9, 0x2a,
    0x0b, 0x50, 0x60, 0x2b, 0x7f, 0x6c, 0x64, 0x80, 0x09, 0x04};

}  // namespace

// static
void OriginTrialsComponentInstallerPolicy::GetComponentHash(
    std::vector<uint8_t>* hash) {
  if (!hash) {
    return;
  }
  hash->assign(std::begin(kOriginTrialSha2Hash),
               std::end(kOriginTrialSha2Hash));
}

void OriginTrialsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetComponentHash(hash);
}

bool OriginTrialsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // Test if the "origin-trials" key is present in the manifest.
  return manifest.contains(kManifestOriginTrialsKey);
}

bool OriginTrialsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OriginTrialsComponentInstallerPolicy::RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
OriginTrialsComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void OriginTrialsComponentInstallerPolicy::OnCustomUninstall() {}

void OriginTrialsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {}

base::FilePath OriginTrialsComponentInstallerPolicy::GetRelativeInstallDir()
    const {
  return base::FilePath(FILE_PATH_LITERAL("OriginTrials"));
}

std::string OriginTrialsComponentInstallerPolicy::GetName() const {
  return "Origin Trials";
}

update_client::InstallerAttributes
OriginTrialsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

}  // namespace component_updater
