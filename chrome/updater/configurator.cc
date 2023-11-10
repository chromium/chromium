// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/configurator.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/enterprise_util.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crx_downloader_factory.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/patch/in_process_patcher.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/in_process_unzipper.h"
#include "components/update_client/unzipper.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#endif

namespace updater {

Configurator::Configurator(scoped_refptr<UpdaterPrefs> prefs,
                           scoped_refptr<ExternalConstants> external_constants)
    : prefs_(prefs),
      policy_service_(base::MakeRefCounted<PolicyService>(external_constants)),
      external_constants_(external_constants),
      activity_data_service_(
          std::make_unique<ActivityDataService>(GetUpdaterScope())),
      unzip_factory_(
          base::MakeRefCounted<update_client::InProcessUnzipperFactory>()),
      patch_factory_(
          base::MakeRefCounted<update_client::InProcessPatcherFactory>()),
      is_managed_device_([] {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
        return base::IsManagedOrEnterpriseDevice();
#else
        return std::nullopt;
#endif
      }()) {
#if BUILDFLAG(IS_LINUX)
  // On Linux creating the NetworkFetcherFactory requires performing blocking IO
  // to load an external library. This should be done when the configurator is
  // created.
  GetNetworkFetcherFactory();
#endif
}
Configurator::~Configurator() = default;

base::TimeDelta Configurator::InitialDelay() const {
  return base::RandDouble() * external_constants_->InitialDelay();
}

base::TimeDelta Configurator::ServerKeepAliveTime() const {
  return std::clamp(external_constants_->ServerKeepAliveTime(),
                     base::Seconds(1), kServerKeepAliveTime);
}

base::TimeDelta Configurator::NextCheckDelay() const {
  PolicyStatus<base::TimeDelta> delay = policy_service_->GetLastCheckPeriod();
  CHECK(delay);
  return delay.policy();
}

base::TimeDelta Configurator::OnDemandDelay() const {
  return base::Seconds(0);
}

base::TimeDelta Configurator::UpdateDelay() const {
  return base::Seconds(0);
}

std::vector<GURL> Configurator::UpdateUrl() const {
  return external_constants_->UpdateURL();
}

std::vector<GURL> Configurator::PingUrl() const {
  return UpdateUrl();
}

GURL Configurator::CrashUploadURL() const {
  return external_constants_->CrashUploadURL();
}

GURL Configurator::DeviceManagementURL() const {
  return external_constants_->DeviceManagementURL();
}

std::string Configurator::GetProdId() const {
  return "updater";
}

base::Version Configurator::GetBrowserVersion() const {
  return version_info::GetVersion();
}

std::string Configurator::GetChannel() const {
  return {};
}

std::string Configurator::GetLang() const {
  return "";
}

std::string Configurator::GetOSLongName() const {
  return std::string(version_info::GetOSType());
}

base::flat_map<std::string, std::string> Configurator::ExtraRequestParams()
    const {
  return {};
}

std::string Configurator::GetDownloadPreference() const {
  PolicyStatus<std::string> preference =
      policy_service_->GetDownloadPreference();
  return preference ? preference.policy() : std::string();
}

scoped_refptr<update_client::NetworkFetcherFactory>
Configurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>(
        PolicyServiceProxyConfiguration::Get(policy_service_));
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
Configurator::GetCrxDownloaderFactory() {
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ =
        updater::MakeCrxDownloaderFactory(GetNetworkFetcherFactory());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
Configurator::GetUnzipperFactory() {
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory> Configurator::GetPatcherFactory() {
  return patch_factory_;
}

bool Configurator::EnabledDeltas() const {
  return external_constants_->EnableDiffUpdates();
}

bool Configurator::EnabledBackgroundDownloader() const {
  return false;
}

bool Configurator::EnabledCupSigning() const {
  return external_constants_->UseCUP();
}

PrefService* Configurator::GetPrefService() const {
  return prefs_->GetPrefService();
}

update_client::ActivityDataService* Configurator::GetActivityDataService()
    const {
  return activity_data_service_.get();
}

bool Configurator::IsPerUserInstall() const {
  return !IsSystemInstall();
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

std::optional<bool> Configurator::IsMachineExternallyManaged() const {
  const std::optional<bool> is_managed_overridden =
      external_constants_->IsMachineManaged();
  return is_managed_overridden.has_value() ? is_managed_overridden
                                           : is_managed_device_;
}

scoped_refptr<PolicyService> Configurator::GetPolicyService() const {
  return policy_service_;
}

crx_file::VerifierFormat Configurator::GetCrxVerifierFormat() const {
  return external_constants_->CrxVerifierFormat();
}

update_client::UpdaterStateProvider Configurator::GetUpdaterStateProvider()
    const {
  return base::BindRepeating([](bool /*is_machine*/) {
    return update_client::UpdaterStateAttributes();
  });
}

std::optional<base::FilePath> Configurator::GetCrxCachePath() const {
  return updater::GetCrxDiffCacheDirectory(GetUpdaterScope());
}

}  // namespace updater
