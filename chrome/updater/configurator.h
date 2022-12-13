// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_CONFIGURATOR_H_
#define CHROME_UPDATER_CONFIGURATOR_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/update_client/buildflags.h"
#include "components/update_client/configurator.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
class ActivityDataService;
class NetworkFetcherFactory;
class CrxDownloaderFactory;
class ProtocolHandlerFactory;
}  // namespace update_client

namespace updater {

class ActivityDataService;
class ExternalConstants;
class PolicyService;
class UpdaterPrefs;

#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
inline constexpr const char* kCrxCachePath = "crx_cache";
#endif

// This class is free-threaded. Its instance is shared by multiple sequences and
// it can't be mutated.
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
  update_client::ActivityDataService* GetActivityDataService() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<update_client::ProtocolHandlerFactory>
  GetProtocolHandlerFactory() const override;
  absl::optional<bool> IsMachineExternallyManaged() const override;
  update_client::UpdaterStateProvider GetUpdaterStateProvider() const override;
#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
  absl::optional<base::FilePath> GetCrxCachePath() const override;
#endif

  base::TimeDelta ServerKeepAliveTime() const;
  scoped_refptr<PolicyService> GetPolicyService() const;
  crx_file::VerifierFormat GetCrxVerifierFormat() const;

 private:
  friend class base::RefCountedThreadSafe<Configurator>;
  ~Configurator() override;

  scoped_refptr<UpdaterPrefs> prefs_;
  scoped_refptr<PolicyService> policy_service_;
  scoped_refptr<ExternalConstants> external_constants_;
  std::unique_ptr<ActivityDataService> activity_data_service_;
  scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<update_client::CrxDownloaderFactory> crx_downloader_factory_;
  scoped_refptr<update_client::UnzipperFactory> unzip_factory_;
  scoped_refptr<update_client::PatcherFactory> patch_factory_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_CONFIGURATOR_H_
