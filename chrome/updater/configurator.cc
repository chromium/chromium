// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/configurator.h"

#include <utility>

#include "base/numerics/ranges.h"
#include "base/rand_util.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/activity.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/crx_downloader_factory.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/updater_scope.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/patch/in_process_patcher.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/in_process_unzipper.h"
#include "components/update_client/unzipper.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/updater/win/net/network.h"
#endif

#if defined(OS_MAC)
#include "chrome/updater/mac/net/network.h"
#endif

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

}  // namespace

namespace updater {

Configurator::Configurator(std::unique_ptr<UpdaterPrefs> prefs)
    : prefs_(std::move(prefs)),
      external_constants_(CreateExternalConstants()),
      activity_data_service_(
          std::make_unique<ActivityDataService>(GetProcessScope())),
      unzip_factory_(
          base::MakeRefCounted<update_client::InProcessUnzipperFactory>()),
      patch_factory_(
          base::MakeRefCounted<update_client::InProcessPatcherFactory>()) {}
Configurator::~Configurator() = default;

double Configurator::InitialDelay() const {
  return base::RandDouble() * external_constants_->InitialDelay();
}

int Configurator::ServerKeepAliveSeconds() const {
  return base::ClampToRange(external_constants_->ServerKeepAliveSeconds(), 1,
                            kServerKeepAliveSeconds);
}

int Configurator::NextCheckDelay() const {
  return 5 * kDelayOneHour;
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

std::string Configurator::GetBrand() const {
  return {};
}

std::string Configurator::GetLang() const {
  return "en-US";
}

std::string Configurator::GetOSLongName() const {
  return version_info::GetOSType();
}

base::flat_map<std::string, std::string> Configurator::ExtraRequestParams()
    const {
  return {{"testrequest", "1"}, {"testsource", "dev"}};
}

std::string Configurator::GetDownloadPreference() const {
  return {};
}

scoped_refptr<update_client::NetworkFetcherFactory>
Configurator::GetNetworkFetcherFactory() {
  if (!network_fetcher_factory_)
    network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>();
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

bool Configurator::EnabledComponentUpdates() const {
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
  return true;
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

}  // namespace updater
