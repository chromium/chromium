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
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crx_downloader_factory.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/persisted_data.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "components/crash/core/common/crash_key.h"
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
      external_constants_(external_constants),
      persisted_data_(base::MakeRefCounted<PersistedData>(
          GetUpdaterScope(),
          prefs->GetPrefService(),
          std::make_unique<ActivityDataService>(GetUpdaterScope()))),
      policy_service_(base::MakeRefCounted<PolicyService>(
          external_constants,
          persisted_data_->GetUsageStatsEnabled())),
      unzip_factory_(
          base::MakeRefCounted<update_client::InProcessUnzipperFactory>()),
      patch_factory_(
          base::MakeRefCounted<update_client::InProcessPatcherFactory>()),
      is_managed_device_([] {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
        return base::IsManagedOrEnterpriseDevice();
#else
        return std::nullopt;
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
      }()) {
#if BUILDFLAG(IS_LINUX)
  // On Linux creating the NetworkFetcherFactory requires performing blocking IO
  // to load an external library. This should be done when the configurator is
  // created.
  GetNetworkFetcherFactory();
#endif
  static crash_reporter::CrashKeyString<6> crash_key_managed("managed");
  crash_key_managed.Set(is_managed_device_ ? "true" : "false");
}
Configurator::~Configurator() = default;

base::TimeDelta Configurator::InitialDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::RandDouble() * external_constants_->InitialDelay();
}

base::TimeDelta Configurator::ServerKeepAliveTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::clamp(external_constants_->ServerKeepAliveTime(),
                    base::Seconds(1), kServerKeepAliveTime);
}

base::TimeDelta Configurator::NextCheckDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PolicyStatus<base::TimeDelta> delay = policy_service_->GetLastCheckPeriod();
  CHECK(delay);
  return delay.policy();
}

base::TimeDelta Configurator::OnDemandDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(0);
}

base::TimeDelta Configurator::UpdateDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Seconds(0);
}

std::vector<GURL> Configurator::UpdateUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->UpdateURL();
}

std::vector<GURL> Configurator::PingUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return UpdateUrl();
}

GURL Configurator::CrashUploadURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->CrashUploadURL();
}

GURL Configurator::DeviceManagementURL() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->DeviceManagementURL();
}

std::string Configurator::GetProdId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "updater";
}

base::Version Configurator::GetBrowserVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return version_info::GetVersion();
}

std::string Configurator::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

std::string Configurator::GetLang() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "";
}

std::string Configurator::GetOSLongName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::string(version_info::GetOSType());
}

base::flat_map<std::string, std::string> Configurator::ExtraRequestParams()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

std::string Configurator::GetDownloadPreference() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PolicyStatus<std::string> preference =
      policy_service_->GetDownloadPreference();
  return preference ? preference.policy() : std::string();
}

scoped_refptr<update_client::NetworkFetcherFactory>
Configurator::GetNetworkFetcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>(
        PolicyServiceProxyConfiguration::Get(policy_service_));
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
Configurator::GetCrxDownloaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!crx_downloader_factory_) {
    crx_downloader_factory_ =
        updater::MakeCrxDownloaderFactory(GetNetworkFetcherFactory());
  }
  return crx_downloader_factory_;
}

scoped_refptr<update_client::UnzipperFactory>
Configurator::GetUnzipperFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return unzip_factory_;
}

scoped_refptr<update_client::PatcherFactory> Configurator::GetPatcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return patch_factory_;
}

bool Configurator::EnabledDeltas() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->EnableDiffUpdates();
}

bool Configurator::EnabledBackgroundDownloader() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

bool Configurator::EnabledCupSigning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->UseCUP();
}

PrefService* Configurator::GetPrefService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return prefs_->GetPrefService();
}

update_client::PersistedData* Configurator::GetPersistedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return persisted_data_.get();
}

scoped_refptr<PersistedData> Configurator::GetUpdaterPersistedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return persisted_data_;
}

bool Configurator::IsPerUserInstall() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !IsSystemInstall();
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

std::optional<bool> Configurator::IsMachineExternallyManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const std::optional<bool> is_managed_overridden =
      external_constants_->IsMachineManaged();
  return is_managed_overridden.has_value() ? is_managed_overridden
                                           : is_managed_device_;
}

scoped_refptr<PolicyService> Configurator::GetPolicyService() const {
  // The policy service is accessed by RPC on a different sequence and this
  // function can't enforce the sequence check for now: crbug.com/1517079.
  return policy_service_;
}

crx_file::VerifierFormat Configurator::GetCrxVerifierFormat() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->CrxVerifierFormat();
}

update_client::UpdaterStateProvider Configurator::GetUpdaterStateProvider()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindRepeating([](bool /*is_machine*/) {
    return update_client::UpdaterStateAttributes();
  });
}

std::optional<base::FilePath> Configurator::GetCrxCachePath() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return updater::GetCrxDiffCacheDirectory(GetUpdaterScope());
}

bool Configurator::IsConnectionMetered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return false;
}

}  // namespace updater
