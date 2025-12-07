// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ping_configurator.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chrome/updater/external_constants.h"
#include "chrome/updater/net/network.h"
#include "chrome/updater/ping_persisted_data.h"
#include "chrome/updater/util/util.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/crx_cache.h"
#include "components/update_client/network.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzipper.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace updater {

namespace {

class PingConfigurator : public update_client::Configurator {
 public:
  explicit PingConfigurator(
      scoped_refptr<ExternalConstants> external_constants);
  PingConfigurator(const PingConfigurator&) = delete;
  PingConfigurator& operator=(const PingConfigurator&) = delete;

  // Overrides for update_client::Configurator.
  base::TimeDelta InitialDelay() const override;
  base::TimeDelta NextCheckDelay() const override;
  base::TimeDelta OnDemandDelay() const override;
  base::TimeDelta UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetLang() const override;
  std::string GetOSLongName() const override;
  base::flat_map<std::string, std::string> ExtraRequestParams() const override;
  std::string GetDownloadPreference() const override;
  scoped_refptr<update_client::NetworkFetcherFactory> GetNetworkFetcherFactory()
      override;
  scoped_refptr<update_client::CrxDownloaderFactory> GetCrxDownloaderFactory()
      override;
  scoped_refptr<update_client::UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<update_client::PatcherFactory> GetPatcherFactory() override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::PersistedData* GetPersistedData() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  std::optional<bool> IsMachineExternallyManaged() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;
  scoped_refptr<update_client::CrxCache> GetCrxCache() const override;
  bool IsConnectionMetered() const override;

 private:
  friend class base::RefCountedThreadSafe<PingConfigurator>;
  ~PingConfigurator() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<ExternalConstants> external_constants_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
};

PingConfigurator::PingConfigurator(
    scoped_refptr<ExternalConstants> external_constants)
    : external_constants_(external_constants),
      persisted_data_(CreatePingPersistedData()) {}

PingConfigurator::~PingConfigurator() = default;

base::TimeDelta PingConfigurator::InitialDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

base::TimeDelta PingConfigurator::NextCheckDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

base::TimeDelta PingConfigurator::OnDemandDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

base::TimeDelta PingConfigurator::UpdateDelay() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::vector<GURL> PingConfigurator::UpdateUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

std::vector<GURL> PingConfigurator::PingUrl() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->UpdateURL();
}

std::string PingConfigurator::GetProdId() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "updater";
}

base::Version PingConfigurator::GetBrowserVersion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return version_info::GetVersion();
}

std::string PingConfigurator::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

std::string PingConfigurator::GetLang() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return "";
}

std::string PingConfigurator::GetOSLongName() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::string(version_info::GetOSType());
}

base::flat_map<std::string, std::string> PingConfigurator::ExtraRequestParams()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

std::string PingConfigurator::GetDownloadPreference() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return {};
}

scoped_refptr<update_client::NetworkFetcherFactory>
PingConfigurator::GetNetworkFetcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!network_fetcher_factory_) {
    network_fetcher_factory_ = base::MakeRefCounted<NetworkFetcherFactory>(
        /*policy_service_proxy_configuration=*/std::nullopt,
        /*event_logger=*/nullptr);
  }
  return network_fetcher_factory_;
}

scoped_refptr<update_client::CrxDownloaderFactory>
PingConfigurator::GetCrxDownloaderFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

scoped_refptr<update_client::UnzipperFactory>
PingConfigurator::GetUnzipperFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

scoped_refptr<update_client::PatcherFactory>
PingConfigurator::GetPatcherFactory() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

bool PingConfigurator::EnabledBackgroundDownloader() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

bool PingConfigurator::EnabledCupSigning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

PrefService* PingConfigurator::GetPrefService() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

update_client::PersistedData* PingConfigurator::GetPersistedData() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return persisted_data_.get();
}

bool PingConfigurator::IsPerUserInstall() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !IsSystemInstall();
}

std::unique_ptr<update_client::ProtocolHandlerFactory>
PingConfigurator::GetProtocolHandlerFactory() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return std::make_unique<update_client::ProtocolHandlerFactoryJSON>();
}

std::optional<bool> PingConfigurator::IsMachineExternallyManaged() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return external_constants_->IsMachineManaged();
}

update_client::UpdaterStateProvider PingConfigurator::GetUpdaterStateProvider()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

scoped_refptr<update_client::CrxCache> PingConfigurator::GetCrxCache() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

bool PingConfigurator::IsConnectionMetered() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NOTREACHED();
}

}  // namespace

scoped_refptr<update_client::Configurator> CreatePingConfigurator() {
  return base::MakeRefCounted<PingConfigurator>(CreateExternalConstants());
}

}  // namespace updater
