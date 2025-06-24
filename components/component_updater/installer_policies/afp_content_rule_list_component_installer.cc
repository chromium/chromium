// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/component_updater/installer_policies/afp_content_rule_list_component_installer.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/component_updater/component_updater_service.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "components/fingerprinting_protection_filter/ios/content_rule_list_data.h"

namespace component_updater {
namespace {

using InstallationResult = ::component_updater::
    AntiFingerprintingContentRuleListComponentInstallerPolicy::
        InstallationResult;

// The SHA256 of the SubjectPublicKeyInfo used to sign the component.
// The CRX ID is: kgdbnmlfakkebekbaceapiaenjgmlhan.
constexpr std::array<uint8_t, 32> kAfpContentRuleListPublicKeySHA256 = {
    0xa6, 0x31, 0xdc, 0xb5, 0x0a, 0xa4, 0x14, 0xa1, 0x02, 0x40, 0xf8,
    0x04, 0xd9, 0x6c, 0xb7, 0x0d, 0x7b, 0xbd, 0x63, 0xf9, 0xc8, 0x65,
    0x6e, 0x9b, 0x83, 0x7a, 0x3a, 0xfd, 0xd1, 0xc8, 0x40, 0xe3};

constexpr char kAfpContentRuleListManifestName[] =
    "Fingerprinting Protection Filter Rules";

constexpr base::FilePath::CharType kWebKitContentRuleListJsonFileName[] =
    FILE_PATH_LITERAL("webkit_content_rule_list.json");

// UMA histogram name for installation results.
constexpr char kInstallationResultHistogramName[] =
    "FingerprintingProtection.WKContentRuleListComponent.InstallationResult";

void WriteMetrics(InstallationResult result) {
  base::UmaHistogramEnumeration(kInstallationResultHistogramName, result);
}

std::optional<std::string> LoadContentRuleListFromDisk(
    const base::FilePath& json_path) {
  if (json_path.empty()) {
    return std::nullopt;
  }

  std::string json;
  if (!base::ReadFileToString(json_path, &json)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    return std::nullopt;
  }
  return json;
}

void PopulateContentRuleListData(std::optional<std::string> json) {
  if (!json) {
    return;
  }
  script_blocking::ContentRuleListData::GetInstance().SetContentRuleList(*json);
}

}  // namespace

AntiFingerprintingContentRuleListComponentInstallerPolicy::
    AntiFingerprintingContentRuleListComponentInstallerPolicy(
        OnLoadCompleteCallback on_load_complete)
    : on_load_complete_(std::move(on_load_complete)) {}

AntiFingerprintingContentRuleListComponentInstallerPolicy::
    ~AntiFingerprintingContentRuleListComponentInstallerPolicy() = default;

// static
base::FilePath
AntiFingerprintingContentRuleListComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kWebKitContentRuleListJsonFileName);
}

bool AntiFingerprintingContentRuleListComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool AntiFingerprintingContentRuleListComponentInstallerPolicy::
    RequiresNetworkEncryption() const {
  return false;
}

update_client::CrxInstaller::Result
AntiFingerprintingContentRuleListComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);
}

void AntiFingerprintingContentRuleListComponentInstallerPolicy::
    OnCustomUninstall() {}

bool AntiFingerprintingContentRuleListComponentInstallerPolicy::
    VerifyInstallation(const base::Value::Dict& manifest,
                       const base::FilePath& install_dir) const {
  const bool install_verified = base::PathExists(GetInstalledPath(install_dir));
  if (install_verified) {
    WriteMetrics(InstallationResult::kSuccess);
  } else {
    WriteMetrics(InstallationResult::kMissingJsonFile);
  }
  return install_verified;
}

void AntiFingerprintingContentRuleListComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  CHECK(!install_dir.empty());

  const base::FilePath json_path = GetInstalledPath(install_dir);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&LoadContentRuleListFromDisk, json_path),
      base::BindOnce(on_load_complete_));
}

base::FilePath AntiFingerprintingContentRuleListComponentInstallerPolicy::
    GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("AntiFingerprintingContentRuleList"));
}

void AntiFingerprintingContentRuleListComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kAfpContentRuleListPublicKeySHA256),
               std::end(kAfpContentRuleListPublicKeySHA256));
}

std::string AntiFingerprintingContentRuleListComponentInstallerPolicy::GetName()
    const {
  return kAfpContentRuleListManifestName;
}

update_client::InstallerAttributes
AntiFingerprintingContentRuleListComponentInstallerPolicy::
    GetInstallerAttributes() const {
  std::string experimental_version;
  if (fingerprinting_protection_filter::features::
          IsFingerprintingProtectionEnabledForIncognitoState(
              /*is_incognito=*/true)) {
    experimental_version =
        fingerprinting_protection_filter::features::kExperimentVersionIncognito
            .Get();
  } else if (fingerprinting_protection_filter::features::
                 IsFingerprintingProtectionEnabledForIncognitoState(
                     /*is_incognito=*/false)) {
    experimental_version = fingerprinting_protection_filter::features::
                               kExperimentVersionNonIncognito.Get();
  }
  return {
      {
          kExperimentalVersionAttributeName,
          experimental_version,
      },
  };
}

void RegisterAntiFingerprintingContentRuleListComponent(
    ComponentUpdateService* cus) {
  if (!fingerprinting_protection_filter::features::
          IsFingerprintingProtectionFeatureEnabled()) {
    return;
  }

  auto policy = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<
          AntiFingerprintingContentRuleListComponentInstallerPolicy>(
          base::BindRepeating(&PopulateContentRuleListData)));
  policy->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
