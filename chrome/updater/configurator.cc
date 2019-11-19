// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/configurator.h"

#include <utility>
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/updater/patcher.h"
#include "chrome/updater/prefs.h"
#include "chrome/updater/unzipper.h"
#include "chrome/updater/updater_constants.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzipper.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "chrome/updater/win/net/network.h"
#endif

namespace {

// Default time constants.
const int kDelayOneMinute = 60;
const int kDelayOneHour = kDelayOneMinute * 60;

}  // namespace

namespace updater {

Configurator::Configurator()
    : pref_service_(CreatePrefService()),
      unzip_factory_(base::MakeRefCounted<UnzipperFactory>()),
      patch_factory_(base::MakeRefCounted<PatcherFactory>()) {}
Configurator::~Configurator() = default;

int Configurator::InitialDelay() const {
  return 0;
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
  return std::vector<GURL>{GURL(kUpdaterJSONDefaultUrl)};
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
#if defined(OS_WIN)
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>();
  }
  return network_fetcher_factory_;
#else
  return nullptr;
#endif
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
  return true;
}

PrefService* Configurator::GetPrefService() const {
  return pref_service_.get();
}

update_client::ActivityDataService* Configurator::GetActivityDataService()
    const {
  return nullptr;
}

bool Configurator::IsPerUserInstall() const {
  return true;
}

std::vector<uint8_t> Configurator::GetRunActionKeyHash() const {
  return {};
}

std::string Configurator::GetAppGuid() const {
  return {};
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
Configurator::GetProtocolHandlerFactory() const {
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

update_client::RecoveryCRXElevator Configurator::GetRecoveryCRXElevator()
    const {
  return {};
}

}  // namespace updater
