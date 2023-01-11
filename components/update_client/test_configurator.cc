// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_configurator.h"

#include <string>
#include <tuple>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/buildflags.h"
#include "components/update_client/crx_downloader_factory.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace update_client {

namespace {

std::vector<GURL> MakeDefaultUrls() {
  std::vector<GURL> urls;
  urls.push_back(GURL(POST_INTERCEPT_SCHEME
                      "://" POST_INTERCEPT_HOSTNAME POST_INTERCEPT_PATH));
  return urls;
}

}  // namespace

TestConfigurator::TestConfigurator(PrefService* pref_service)
    : enabled_cup_signing_(false),
      pref_service_(pref_service),
      unzip_factory_(base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))),
      patch_factory_(base::MakeRefCounted<update_client::PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))),
      test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      network_fetcher_factory_(
          base::MakeRefCounted<NetworkFetcherChromiumFactory>(
              test_shared_loader_factory_,
              base::BindRepeating([](const GURL& url) { return false; }))),
      updater_state_provider_(base::BindRepeating(
          [](bool /*is_machine*/) { return UpdaterStateAttributes(); })) {
  std::ignore = crx_cache_root_temp_dir_.CreateUniqueTempDir();
}

TestConfigurator::~TestConfigurator() = default;

base::TimeDelta TestConfigurator::InitialDelay() const {
  return initial_time_;
}

base::TimeDelta TestConfigurator::NextCheckDelay() const {
  return base::Seconds(1);
}

base::TimeDelta TestConfigurator::OnDemandDelay() const {
  return ondemand_time_;
}

base::TimeDelta TestConfigurator::UpdateDelay() const {
  return base::Seconds(1);
}

std::vector<GURL> TestConfigurator::UpdateUrl() const {
  if (!update_check_urls_.empty())
    return update_check_urls_;

  return MakeDefaultUrls();
}

std::vector<GURL> TestConfigurator::PingUrl() const {
  if (!ping_url_.is_empty())
    return std::vector<GURL>(1, ping_url_);

  return UpdateUrl();
}

std::string TestConfigurator::GetProdId() const {
  return "fake_prodid";
}

base::Version TestConfigurator::GetBrowserVersion() const {
  // Needs to be larger than the required version in tested component manifests.
  return base::Version("30.0");
}

std::string TestConfigurator::GetChannel() const {
  return "fake_channel_string";
}

std::string TestConfigurator::GetLang() const {
  return "fake_lang";
}

std::string TestConfigurator::GetOSLongName() const {
  return "Fake Operating System";
}

base::flat_map<std::string, std::string> TestConfigurator::ExtraRequestParams()
    const {
  return {{"extra", "foo"}};
}

std::string TestConfigurator::GetDownloadPreference() const {
  return download_preference_;
}

scoped_refptr<NetworkFetcherFactory>
TestConfigurator::GetNetworkFetcherFactory() {
  return network_fetcher_factory_;
}

scoped_refptr<CrxDownloaderFactory>
TestConfigurator::GetCrxDownloaderFactory() {
  return crx_downloader_factory_;
}

scoped_refptr<UnzipperFactory> TestConfigurator::GetUnzipperFactory() {
  return unzip_factory_;
}

scoped_refptr<PatcherFactory> TestConfigurator::GetPatcherFactory() {
  return patch_factory_;
}

bool TestConfigurator::EnabledDeltas() const {
  return true;
}

bool TestConfigurator::EnabledBackgroundDownloader() const {
  return false;
}

bool TestConfigurator::EnabledCupSigning() const {
  return enabled_cup_signing_;
}

PrefService* TestConfigurator::GetPrefService() const {
  return pref_service_;
}

ActivityDataService* TestConfigurator::GetActivityDataService() const {
  return nullptr;
}

bool TestConfigurator::IsPerUserInstall() const {
  return true;
}

std::unique_ptr<ProtocolHandlerFactory>
TestConfigurator::GetProtocolHandlerFactory() const {
  return std::make_unique<ProtocolHandlerFactoryJSON>();
}

absl::optional<bool> TestConfigurator::IsMachineExternallyManaged() const {
  return is_machine_externally_managed_;
}

UpdaterStateProvider TestConfigurator::GetUpdaterStateProvider() const {
  return updater_state_provider_;
}

#if BUILDFLAG(ENABLE_PUFFIN_PATCHES)
absl::optional<base::FilePath> TestConfigurator::GetCrxCachePath() const {
  if (!crx_cache_root_temp_dir_.IsValid()) {
    return absl::nullopt;
  }
  return absl::optional<base::FilePath>(
      crx_cache_root_temp_dir_.GetPath().AppendASCII("crx_cache"));
}
#endif

void TestConfigurator::SetOnDemandTime(base::TimeDelta time) {
  ondemand_time_ = time;
}

void TestConfigurator::SetInitialDelay(base::TimeDelta delay) {
  initial_time_ = delay;
}

void TestConfigurator::SetEnabledCupSigning(bool enabled_cup_signing) {
  enabled_cup_signing_ = enabled_cup_signing;
}

void TestConfigurator::SetDownloadPreference(
    const std::string& download_preference) {
  download_preference_ = download_preference;
}

void TestConfigurator::SetUpdateCheckUrl(const GURL& url) {
  update_check_urls_ = {url};
}

void TestConfigurator::SetUpdateCheckUrls(const std::vector<GURL>& urls) {
  update_check_urls_ = urls;
}

void TestConfigurator::SetPingUrl(const GURL& url) {
  ping_url_ = url;
}

void TestConfigurator::SetCrxDownloaderFactory(
    scoped_refptr<CrxDownloaderFactory> crx_downloader_factory) {
  crx_downloader_factory_ = crx_downloader_factory;
}

void TestConfigurator::SetIsMachineExternallyManaged(
    absl::optional<bool> is_machine_externally_managed) {
  is_machine_externally_managed_ = is_machine_externally_managed;
}

void TestConfigurator::SetUpdaterStateProvider(
    UpdaterStateProvider update_state_provider) {
  updater_state_provider_ = update_state_provider;
}

}  // namespace update_client
