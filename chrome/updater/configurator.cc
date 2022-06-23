// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/configurator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/cxx17_backports.h"
#include "base/enterprise_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/rand_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crx_downloader_factory.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/policy/service.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"
#include "components/crx_file/crx_verifier.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/patch/in_process_patcher.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/in_process_unzipper.h"
#include "components/update_client/unzipper.h"
#include "components/version_info/version_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "chrome/updater/win/net/network.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/updater/mac/net/network.h"
#elif BUILDFLAG(IS_LINUX)
#include "chrome/updater/linux/net/network.h"
#endif

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

}  // namespace

namespace updater {

Configurator::Configurator(scoped_refptr<UpdaterPrefs> prefs,
                           scoped_refptr<ExternalConstants> external_constants)
    : prefs_(prefs),
      policy_service_(PolicyService::Create(external_constants)),
      external_constants_(external_constants),
      activity_data_service_(
          std::make_unique<ActivityDataService>(GetUpdaterScope())),
      unzip_factory_(
          base::MakeRefCounted<update_client::InProcessUnzipperFactory>()),
      patch_factory_(
          base::MakeRefCounted<update_client::InProcessPatcherFactory>()) {}
Configurator::~Configurator() = default;

double Configurator::InitialDelay() const {
  return base::RandDouble() * external_constants_->InitialDelay();
}

int Configurator::ServerKeepAliveSeconds() const {
  return base::clamp(external_constants_->ServerKeepAliveSeconds(), 1,
                     kServerKeepAliveSeconds);
}

int Configurator::NextCheckDelay() const {
  int minutes = 0;
  return policy_service_->GetLastCheckPeriodMinutes(nullptr, &minutes)
             ? minutes * kDelayOneMinute
             : 4 * kDelayOneHour + 30 * kDelayOneMinute;
}

int Configurator::OnDemandDelay() const {
  return 0;
}

int Configurator::UpdateDelay() const {
  return 0;
}

std::vector<GURL> Configurator::UpdateUrl() const {
  return external_constants_->UpdateURL();
}

std::vector<GURL> Configurator::PingUrl() const {
  return UpdateUrl();
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
  return version_info::GetOSType();
}

base::flat_map<std::string, std::string> Configurator::ExtraRequestParams()
    const {
  return {};
}

std::string Configurator::GetDownloadPreference() const {
  std::string preference;
  return policy_service_->GetDownloadPreferenceGroupPolicy(nullptr, &preference)
             ? preference
             : std::string();
}

scoped_refptr<update_client::NetworkFetcherFactory>
Configurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_)
    network_fetcher_factory_ =
        base::MakeRefCounted<NetworkFetcherFactory>(GetPolicyService());
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
  return false;
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
  switch (GetUpdaterScope()) {
    case UpdaterScope::kSystem:
      return false;
    case UpdaterScope::kUser:
      return true;
  }
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

absl::optional<bool> Configurator::IsMachineExternallyManaged() const {
#if BUILDFLAG(IS_WIN)
  // TODO (crbug.com/1320776): For legacy compatibility, this uses
  // base::IsEnrolledToDomain(). It cannot use IsEnterpriseDevice() because
  // checking for AAD-join status involves a potentially blocking which is
  // currently not allowed in this method.
  // Consider whether this should use IsManagedDevice() instead.
  return base::win::IsEnrolledToDomain();
#elif BUILDFLAG(IS_MAC)
  // TODO (crbug.com/1320776): For legacy compatibility, this uses
  // IsEnterpriseDevice() which effectively equates to a domain join check.
  // IsManagedDevice() involves potentially blocking calls which are currently
  // not allowed in this method.
  // Consider whether this should use IsManagedDevice() instead.
  return base::IsEnterpriseDevice();
#else
  return absl::nullopt;
#endif
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

}  // namespace updater
