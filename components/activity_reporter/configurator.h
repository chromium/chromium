// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTIVITY_REPORTER_CONFIGURATOR_H_
#define COMPONENTS_ACTIVITY_REPORTER_CONFIGURATOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/version_info/channel.h"
#include "components/update_client/configurator.h"

class PrefService;

namespace update_client {
class CrxCache;
class CrxDownloaderFactory;
class NetworkFetcherFactory;
class PatcherFactory;
class PersistedData;
class ProtocolHandlerFactory;
class UnzipperFactory;
}  // namespace update_client

namespace activity_reporter {

class ActivityReporterConfigurator final : public update_client::Configurator {
 public:
  ActivityReporterConfigurator(
      base::RepeatingCallback<PrefService*()> pref_service_provider,
      scoped_refptr<update_client::NetworkFetcherFactory>
          network_fetcher_factory,
      base::RepeatingCallback<version_info::Channel()> channel_provider,
      bool per_user_install);
  ActivityReporterConfigurator(const ActivityReporterConfigurator&) = delete;
  ActivityReporterConfigurator& operator=(
      const ActivityReporterConfigurator&&) = delete;

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
  friend class base::RefCountedThreadSafe<ActivityReporterConfigurator>;
  ~ActivityReporterConfigurator() override;

  SEQUENCE_CHECKER(sequence_checker_);
  base::RepeatingCallback<PrefService*()> pref_service_provider_;
  std::unique_ptr<update_client::PersistedData> persisted_data_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  base::RepeatingCallback<version_info::Channel()> channel_provider_;
  const bool per_user_install_;
};

}  // namespace activity_reporter

#endif  // COMPONENTS_ACTIVITY_REPORTER_CONFIGURATOR_H_
