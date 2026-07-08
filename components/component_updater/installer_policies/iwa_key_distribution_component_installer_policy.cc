// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/component_updater/installer_policies/iwa_key_distribution_component_installer_policy.h"

#include <cstdint>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "components/component_updater/component_installer.h"
#include "components/component_updater/component_updater_service.h"
#include "components/crx_file/id_util.h"
#include "components/webapps/isolated_web_apps/key_distribution/iwa_key_distribution_info_provider.h"

namespace {

constexpr std::string_view kPreloadedKey = "is_preloaded";
constexpr std::string_view kIwaKdcExpCohortAttribute = "_iwa_kdc_exp_cohort";

component_updater::OnDemandUpdater::Priority GetOnDemandUpdatePriority() {
#if BUILDFLAG(IS_WIN)
  return component_updater::OnDemandUpdater::Priority::FOREGROUND;
#else
  return component_updater::OnDemandUpdater::Priority::BACKGROUND;
#endif
}

}  // namespace

namespace component_updater {

IwaKeyDistributionComponentInstallerPolicy::
    IwaKeyDistributionComponentInstallerPolicy(
        base::RepeatingCallback<
            void(base::PassKey<web_app::IwaKeyDistributionInfoProvider>)>
            queue_on_demand_update) {
  if (queue_on_demand_update) {
    // `RegisterIwaKeyDistributionComponent` is effectively called before the
    // user profile is created. Hence we can avoid eventual initialization race
    // conditions for user sessions.
    web_app::IwaKeyDistributionInfoProvider::GetInstance(
        base::PassKey<IwaKeyDistributionComponentInstallerPolicy>())
        .SetUp(std::move(queue_on_demand_update));
  }
}

// static
void IwaKeyDistributionComponentInstallerPolicy::QueueOnDemandUpdate(
    OnDemandUpdater& updater) {
  updater.OnDemandUpdate(
      crx_file::id_util::GenerateIdFromHash(kIwaKeyDistributionPublicKeySHA256),
      GetOnDemandUpdatePriority(), base::BindOnce([](update_client::Error err) {
        VLOG(1) << "On-demand update for the Iwa Key Distribution Component "
                   "finished with result "
                << std::to_underlying(err);
      }));
}

bool IwaKeyDistributionComponentInstallerPolicy::VerifyInstallation(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) const {
  return base::PathExists(install_dir.Append(kDataFileName));
}

bool IwaKeyDistributionComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  // Returning `false` here means that updates to this component cannot be
  // disabled by the `ComponentUpdatesEnabled` policy. This exemption is
  // warranted for IWA key distribution because this component is essential for
  // securely installing and managing only legitimate Isolated Web Apps.
  // The component is data-only (not executable code) and includes crucial
  // allowlist, blocklist and key rotation information.
  return false;
}

bool IwaKeyDistributionComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
IwaKeyDistributionComponentInstallerPolicy::OnCustomInstall(
    const base::DictValue& manifest,
    const base::FilePath& install_dir) {
  // No custom install.
  return update_client::CrxInstaller::Result(0);
}

void IwaKeyDistributionComponentInstallerPolicy::OnCustomUninstall() {}

void IwaKeyDistributionComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::DictValue manifest) {
  if (install_dir.empty() || !version.IsValid()) {
    return;
  }

  VLOG(1) << "Iwa Key Distribution Component ready, version " << version
          << " in " << install_dir;
  web_app::IwaKeyDistributionInfoProvider::GetInstance(
      base::PassKey<IwaKeyDistributionComponentInstallerPolicy>())
      .LoadKeyDistributionData(
          version, install_dir.Append(kDataFileName),
          /*is_preloaded=*/manifest.FindBool(kPreloadedKey).value_or(false));
}

base::FilePath
IwaKeyDistributionComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(kRelativeInstallDirName);
}

void IwaKeyDistributionComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kIwaKeyDistributionPublicKeySHA256),
               std::end(kIwaKeyDistributionPublicKeySHA256));
}

std::string IwaKeyDistributionComponentInstallerPolicy::GetName() const {
  return kManifestName;
}

update_client::InstallerAttributes
IwaKeyDistributionComponentInstallerPolicy::GetInstallerAttributes() const {
  update_client::InstallerAttributes attributes;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kIwaKeyDistributionComponentExpCohort)) {
    attributes.emplace(
        kIwaKdcExpCohortAttribute,
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kIwaKeyDistributionComponentExpCohort));
  }
  return attributes;
}

}  // namespace component_updater
