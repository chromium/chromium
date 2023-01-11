// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/client_side_phishing_component_installer_policy.h"

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"

namespace component_updater {

namespace {
const char kClientSidePhishingManifestName[] = "Client Side Phishing Detection";

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: imefjhfbkmcmebodilednhmaccmincoa
const uint8_t kClientSidePhishingPublicKeySHA256[32] = {
    0x8c, 0x45, 0x97, 0x51, 0xac, 0x2c, 0x41, 0xe3, 0x8b, 0x43, 0xd7,
    0xc0, 0x22, 0xc8, 0xd2, 0xe0, 0xe3, 0xe2, 0x33, 0x88, 0x1f, 0x09,
    0x6d, 0xde, 0x65, 0x6a, 0x83, 0x32, 0x71, 0x52, 0x6e, 0x77};

}  // namespace

const base::FilePath::CharType kClientModelBinaryPbFileName[] =
    FILE_PATH_LITERAL("client_model.pb");
const base::FilePath::CharType kVisualTfLiteModelFileName[] =
    FILE_PATH_LITERAL("visual_model.tflite");

ClientSidePhishingComponentInstallerPolicy::
    ClientSidePhishingComponentInstallerPolicy(
        const ReadFilesCallback& read_files_callback,
        const InstallerAttributesCallback& installer_attributes_callback)
    : read_files_callback_(std::move(read_files_callback)),
      installer_attributes_callback_(std::move(installer_attributes_callback)) {
}

ClientSidePhishingComponentInstallerPolicy::
    ~ClientSidePhishingComponentInstallerPolicy() = default;

// static
void ClientSidePhishingComponentInstallerPolicy::GetPublicHash(
    std::vector<uint8_t>* hash) {
  hash->assign(kClientSidePhishingPublicKeySHA256,
               kClientSidePhishingPublicKeySHA256 +
                   std::size(kClientSidePhishingPublicKeySHA256));
}

bool ClientSidePhishingComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool ClientSidePhishingComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
ClientSidePhishingComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void ClientSidePhishingComponentInstallerPolicy::OnCustomUninstall() {}

void ClientSidePhishingComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  read_files_callback_.Run(install_dir);
}

// Called during startup and installation before ComponentReady().
bool ClientSidePhishingComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  // No need to actually validate the proto here, since we'll do the checking
  // in PopulateFromDynamicUpdate().
  return base::PathExists(install_dir.Append(kClientModelBinaryPbFileName)) ||
         base::PathExists(install_dir.Append(kVisualTfLiteModelFileName));
}

base::FilePath
ClientSidePhishingComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("ClientSidePhishing"));
}

void ClientSidePhishingComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  GetPublicHash(hash);
}

std::string ClientSidePhishingComponentInstallerPolicy::GetName() const {
  return kClientSidePhishingManifestName;
}

update_client::InstallerAttributes
ClientSidePhishingComponentInstallerPolicy::GetInstallerAttributes() const {
  return installer_attributes_callback_.Run();
}

}  // namespace component_updater
