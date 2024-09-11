// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_TEST_CONFIGURATOR_H_
#define COMPONENTS_UPDATE_CLIENT_TEST_CONFIGURATOR_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/update_client/configurator.h"
#include "services/network/test/test_url_loader_factory.h"
#include "url/gurl.h"

class PrefService;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace update_client {

class CrxDownloaderFactory;
class NetworkFetcherFactory;
class PatchChromiumFactory;
class ProtocolHandlerFactory;
class TestActivityDataService;
class UnzipChromiumFactory;

#define POST_INTERCEPT_SCHEME "https"
#define POST_INTERCEPT_HOSTNAME "localhost2"
#define POST_INTERCEPT_PATH "/update2"

// component 1 has extension id "jebgalgnebhfojomionfpkfelancnnkf", and
// the RSA public key the following hash:
const uint8_t jebg_hash[] = {0x94, 0x16, 0x0b, 0x6d, 0x41, 0x75, 0xe9, 0xec,
                             0x8e, 0xd5, 0xfa, 0x54, 0xb0, 0xd2, 0xdd, 0xa5,
                             0x6e, 0x05, 0x6b, 0xe8, 0x73, 0x47, 0xf6, 0xc4,
                             0x11, 0x9f, 0xbc, 0xb3, 0x09, 0xb3, 0x5b, 0x40};
// component 1 public key (base64 encoded):
inline constexpr char jebg_public_key[] =
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQC68bW8i/RzSaeXOcNLuBw0SP9+1bdo5ysLqH"
    "qfLqZs6XyJWEyL0U6f1axPR6LwViku21kgdc6PI524eb8Cr+a/iXGgZ8SdvZTcfQ/g/ukwlblF"
    "mtqYfDoVpz03U8rDQ9b6DxeJBF4r48TNlFORggrAiNR26qbf1i178Au12AzWtwIDAQAB";

// component 2 has extension id "abagagagagagagagagagagagagagagag", and
// the RSA public key the following hash:
inline constexpr uint8_t abag_hash[] = {
    0x01, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06,
    0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x01};
// component 3 has extension id "ihfokbkgjpifnbbojhneepfflplebdkc", and
// the RSA public key the following hash:
inline constexpr uint8_t ihfo_hash[] = {
    0x87, 0x5e, 0xa1, 0xa6, 0x9f, 0x85, 0xd1, 0x1e, 0x97, 0xd4, 0x4f,
    0x55, 0xbf, 0xb4, 0x13, 0xa2, 0xe7, 0xc5, 0xc8, 0xf5, 0x60, 0x19,
    0x78, 0x1b, 0x6d, 0xe9, 0x4c, 0xeb, 0x96, 0x05, 0x42, 0x17};

// runaction_test_win.crx and its payload id: gjpmebpgbhcamgdgjcmnjfhggjpgcimm
inline constexpr uint8_t gjpm_hash[] = {
    0x69, 0xfc, 0x41, 0xf6, 0x17, 0x20, 0xc6, 0x36, 0x92, 0xcd, 0x95,
    0x76, 0x69, 0xf6, 0x28, 0xcc, 0xbe, 0x98, 0x4b, 0x93, 0x17, 0xd6,
    0x9c, 0xb3, 0x64, 0x0c, 0x0d, 0x25, 0x61, 0xc5, 0x80, 0x1d};

class TestConfigurator : public Configurator {
 public:
  explicit TestConfigurator(PrefService* pref_service);
  TestConfigurator(const TestConfigurator&) = delete;
  TestConfigurator& operator=(const TestConfigurator&) = delete;

  TestActivityDataService* GetActivityDataService() const;

  // Overrides for Configurator.
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
  scoped_refptr<NetworkFetcherFactory> GetNetworkFetcherFactory() override;
  scoped_refptr<CrxDownloaderFactory> GetCrxDownloaderFactory() override;
  scoped_refptr<UnzipperFactory> GetUnzipperFactory() override;
  scoped_refptr<PatcherFactory> GetPatcherFactory() override;
  bool EnabledDeltas() const override;
  bool EnabledBackgroundDownloader() const override;
  bool EnabledCupSigning() const override;
  PrefService* GetPrefService() const override;
  PersistedData* GetPersistedData() const override;
  bool IsPerUserInstall() const override;
  std::unique_ptr<ProtocolHandlerFactory> GetProtocolHandlerFactory()
      const override;
  std::optional<bool> IsMachineExternallyManaged() const override;
  UpdaterStateProvider GetUpdaterStateProvider() const override;
  std::optional<base::FilePath> GetCrxCachePath() const override;
  bool IsConnectionMetered() const override;

  void SetOnDemandTime(base::TimeDelta seconds);
  void SetInitialDelay(base::TimeDelta seconds);
  void SetDownloadPreference(const std::string& download_preference);
  void SetEnabledCupSigning(bool use_cup_signing);
  void SetUpdateCheckUrl(const GURL& url);
  void SetUpdateCheckUrls(const std::vector<GURL>& urls);
  void SetPingUrl(const GURL& url);
  void SetCrxDownloaderFactory(
      scoped_refptr<CrxDownloaderFactory> crx_downloader_factory);
  void SetIsMachineExternallyManaged(
      std::optional<bool> is_machine_externally_managed);
  void SetIsNetworkConnectionMetered(bool is_network_connection_metered);
  void SetUpdaterStateProvider(UpdaterStateProvider update_state_provider);
  network::TestURLLoaderFactory* test_url_loader_factory() {
    return &test_url_loader_factory_;
  }

 private:
  friend class base::RefCountedThreadSafe<TestConfigurator>;
  ~TestConfigurator() override;

  class TestPatchService;

  SEQUENCE_CHECKER(sequence_checker_);
  base::TimeDelta initial_time_ = base::Seconds(0);
  base::TimeDelta ondemand_time_ = base::Seconds(0);
  std::string download_preference_;
  bool enabled_cup_signing_;
  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<PersistedData> persisted_data_;
  raw_ptr<TestActivityDataService> activity_data_service_;
  std::vector<GURL> update_check_urls_;
  GURL ping_url_;
  scoped_refptr<update_client::UnzipChromiumFactory> unzip_factory_;
  scoped_refptr<update_client::PatchChromiumFactory> patch_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<NetworkFetcherFactory> network_fetcher_factory_;
  scoped_refptr<CrxDownloaderFactory> crx_downloader_factory_;
  UpdaterStateProvider updater_state_provider_;
  std::optional<bool> is_machine_externally_managed_;
  bool is_network_connection_metered_;
  base::ScopedTempDir crx_cache_root_temp_dir_;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_TEST_CONFIGURATOR_H_
