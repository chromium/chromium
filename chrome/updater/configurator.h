// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONFIGURATOR_H_
#define CHROME_UPDATER_CONFIGURATOR_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/update_client/configurator.h"

class GURL;
class PrefService;

namespace base {
class Version;
class FilePath;
}  // namespace base

namespace crx_file {
enum class VerifierFormat;
}

namespace update_client {
class NetworkFetcherFactory;
class CrxDownloaderFactory;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace updater {

class ExternalConstants;
class PersistedData;
class PolicyService;
class UpdaterPrefs;

class Configurator : public update_client::Configurator {
 public:
  Configurator(scoped_refptr<UpdaterPrefs> prefs,
               scoped_refptr<ExternalConstants> external_constants);
  Configurator(const Configurator&) = delete;
  Configurator& operator=(const Configurator&) = delete;

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
  bool EnabledDeltas() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  update_client::PersistedData* GetPersistedData() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  std::optional<bool> IsMachineExternallyManaged() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;
  std::optional<base::FilePath> GetCrxCachePath() const override;
  bool IsConnectionMetered() const override;

  scoped_refptr<PersistedData> GetUpdaterPersistedData() const;
  virtual GURL CrashUploadURL() const;
  virtual GURL DeviceManagementURL() const;

  base::TimeDelta ServerKeepAliveTime() const;
  scoped_refptr<PolicyService> GetPolicyService() const;
  crx_file::VerifierFormat GetCrxVerifierFormat() const;

 private:
  friend class base::RefCountedThreadSafe<Configurator>;
  ~Configurator() override;

  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<UpdaterPrefs> prefs_;
  scoped_refptr<ExternalConstants> external_constants_;
  scoped_refptr<PersistedData> persisted_data_;
  scoped_refptr<PolicyService> policy_service_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;
  const std::optional<bool> is_managed_device_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CONFIGURATOR_H_
