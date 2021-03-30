// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONFIGURATOR_H_
#define CHROME_UPDATER_CONFIGURATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "components/update_client/configurator.h"

class GURL;
class PrefService;

namespace base {
class Version;
}  // namespace base

namespace update_client {
class ActivityDataService;
class NetworkFetcherFactory;
class CrxDownloaderFactory;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace updater {

class ActivityDataService;
class UpdaterPrefs;
class ExternalConstants;

class Configurator : public update_client::Configurator {
 public:
  explicit Configurator(std::unique_ptr<UpdaterPrefs> prefs);
  Configurator(const Configurator&) = delete;
  Configurator& operator=(const Configurator&) = delete;

  // Configurator for update_client::Configurator.
  double InitialDelay() const override;
  int NextCheckDelay() const override;
  int OnDemandDelay() const override;
  int UpdateDelay() const override;
  std::vector<GURL> UpdateUrl() const override;
  std::vector<GURL> PingUrl() const override;
  std::string GetProdId() const override;
  base::Version GetBrowserVersion() const override;
  std::string GetChannel() const override;
  std::string GetBrand() const override;
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
  bool EnabledDeltas() const override;
  bool EnabledComponentUpdates() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  int ServerKeepAliveSeconds() const;

 private:
  friend class base::RefCountedThreadSafe<Configurator>;
  ~Configurator() override;

  std::unique_ptr<UpdaterPrefs> prefs_;
  std::unique_ptr<ExternalConstants> external_constants_;
  std::unique_ptr<ActivityDataService> activity_data_service_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CONFIGURATOR_H_
