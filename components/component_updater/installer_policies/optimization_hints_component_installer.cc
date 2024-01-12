// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/optimization_hints_component_installer.h"

#include <utility>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/values.h"
#include "base/version.h"
#include "components/component_updater/component_updater_paths.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_hints_component_update_listener.h"

using component_updater::ComponentUpdateService;

namespace component_updater {

namespace {

const char kDisableInstallerUpdate[] = "optimization-guide-disable-installer";

// The extension id is: lmelglejhemejginpboagddgdfbepgmp
const uint8_t kOptimizationHintsPublicKeySHA256[32] = {
    0xbc, 0x4b, 0x6b, 0x49, 0x74, 0xc4, 0x96, 0x8d, 0xf1, 0xe0, 0x63,
    0x36, 0x35, 0x14, 0xf6, 0xcf, 0x86, 0x92, 0xe6, 0x06, 0x03, 0x76,
    0x70, 0xaf, 0x8b, 0xd4, 0x47, 0x2c, 0x42, 0x59, 0x38, 0xef};

const char kOptimizationHintsSetFetcherManifestName[] = "Optimization Hints";

}  // namespace

// static
const char
    OptimizationHintsComponentInstallerPolicy::kManifestRulesetFormatKey[] =
        "ruleset_format";

OptimizationHintsComponentInstallerPolicy::
    OptimizationHintsComponentInstallerPolicy()
    : ruleset_format_version_(
          base::Version(optimization_guide::kRulesetFormatVersionString)) {
  DCHECK(ruleset_format_version_.IsValid());
}

OptimizationHintsComponentInstallerPolicy::
    ~OptimizationHintsComponentInstallerPolicy() = default;

bool OptimizationHintsComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool OptimizationHintsComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
OptimizationHintsComponentInstallerPolicy::OnCustomInstall(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void OptimizationHintsComponentInstallerPolicy::OnCustomUninstall() {}

void OptimizationHintsComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  DCHECK(!install_dir.empty());
  DVLOG(1) << "Optimization Hints Version Ready: " << version.GetString();
  optimization_guide::OptimizationHintsComponentUpdateListener*
      update_listener = optimization_guide::
          OptimizationHintsComponentUpdateListener::GetInstance();
  if (update_listener && !base::CommandLine::ForCurrentProcess()->HasSwitch(
                             kDisableInstallerUpdate)) {
    optimization_guide::HintsComponentInfo info(
        version,
        install_dir.Append(optimization_guide::kUnindexedHintsFileName));
    update_listener->MaybeUpdateHintsComponent(info);
  }
}

// Called during startup and installation before ComponentReady().
bool OptimizationHintsComponentInstallerPolicy::VerifyInstallation(
    const base::Value::Dict& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir);
}

base::FilePath
OptimizationHintsComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("OptimizationHints"));
}

void OptimizationHintsComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  if (!hash) {
    return;
  }
  hash->assign(std::begin(kOptimizationHintsPublicKeySHA256),
               std::end(kOptimizationHintsPublicKeySHA256));
}

std::string OptimizationHintsComponentInstallerPolicy::GetName() const {
  return kOptimizationHintsSetFetcherManifestName;
}

update_client::InstallerAttributes
OptimizationHintsComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterOptimizationHintsComponent(ComponentUpdateService* cus) {
  if (!optimization_guide::features::IsOptimizationHintsEnabled()) {
    return;
  }

  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<OptimizationHintsComponentInstallerPolicy>());
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
