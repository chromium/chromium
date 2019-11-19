// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/test_configurator.h"

#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "components/prefs/pref_service.h"
#include "components/services/patch/in_process_file_patcher.h"
#include "components/services/unzip/in_process_unzipper.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/net/network_chromium.h"
#include "components/update_client/patch/patch_impl.h"
#include "components/update_client/patcher.h"
#include "components/update_client/protocol_handler.h"
#include "components/update_client/unzip/unzip_impl.h"
#include "components/update_client/unzipper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
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

TestConfigurator::TestConfigurator()
    : brand_("TEST"),
      initial_time_(0),
      ondemand_time_(0),
      enabled_cup_signing_(false),
      enabled_component_updates_(true),
      unzip_factory_(base::MakeRefCounted<update_client::UnzipChromiumFactory>(
          base::BindRepeating(&unzip::LaunchInProcessUnzipper))),
      patch_factory_(base::MakeRefCounted<update_client::PatchChromiumFactory>(
          base::BindRepeating(&patch::LaunchInProcessFilePatcher))),
      test_shared_loader_factory_(
          base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
              &test_url_loader_factory_)),
      network_fetcher_factory_(
          base::MakeRefCounted<NetworkFetcherChromiumFactory>(
              test_shared_loader_factory_)) {}

TestConfigurator::~TestConfigurator() {
}

int TestConfigurator::InitialDelay() const {
  return initial_time_;
}

int TestConfigurator::NextCheckDelay() const {
  return 1;
}

int TestConfigurator::OnDemandDelay() const {
  return ondemand_time_;
}

int TestConfigurator::UpdateDelay() const {
  return 1;
}

std::vector<GURL> TestConfigurator::UpdateUrl() const {
  if (!update_check_url_.is_empty())
    return std::vector<GURL>(1, update_check_url_);

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

std::string TestConfigurator::GetBrand() const {
  return brand_;
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

scoped_refptr<UnzipperFactory> TestConfigurator::GetUnzipperFactory() {
  return unzip_factory_;
}

scoped_refptr<PatcherFactory> TestConfigurator::GetPatcherFactory() {
  return patch_factory_;
}

bool TestConfigurator::EnabledDeltas() const {
  return true;
}

bool TestConfigurator::EnabledComponentUpdates() const {
  return enabled_component_updates_;
}

bool TestConfigurator::EnabledBackgroundDownloader() const {
  return false;
}

bool TestConfigurator::EnabledCupSigning() const {
  return enabled_cup_signing_;
}

void TestConfigurator::SetBrand(const std::string& brand) {
  brand_ = brand;
}

void TestConfigurator::SetOnDemandTime(int seconds) {
  ondemand_time_ = seconds;
}

void TestConfigurator::SetInitialDelay(int seconds) {
  initial_time_ = seconds;
}

void TestConfigurator::SetEnabledCupSigning(bool enabled_cup_signing) {
  enabled_cup_signing_ = enabled_cup_signing;
}

void TestConfigurator::SetEnabledComponentUpdates(
    bool enabled_component_updates) {
  enabled_component_updates_ = enabled_component_updates;
}

void TestConfigurator::SetDownloadPreference(
    const std::string& download_preference) {
  download_preference_ = download_preference;
}

void TestConfigurator::SetUpdateCheckUrl(const GURL& url) {
  update_check_url_ = url;
}

void TestConfigurator::SetPingUrl(const GURL& url) {
  ping_url_ = url;
}

void TestConfigurator::SetAppGuid(const std::string& app_guid) {
  app_guid_ = app_guid;
}

PrefService* TestConfigurator::GetPrefService() const {
  return nullptr;
}

ActivityDataService* TestConfigurator::GetActivityDataService() const {
  return nullptr;
}

bool TestConfigurator::IsPerUserInstall() const {
  return true;
}

std::vector<uint8_t> TestConfigurator::GetRunActionKeyHash() const {
  return std::vector<uint8_t>(std::begin(gjpm_hash), std::end(gjpm_hash));
}

std::string TestConfigurator::GetAppGuid() const {
  return app_guid_;
}

std::unique_ptr<ProtocolHandlerFactory>
TestConfigurator::GetProtocolHandlerFactory() const {
  return std::make_unique<ProtocolHandlerFactoryJSON>();
}

RecoveryCRXElevator TestConfigurator::GetRecoveryCRXElevator() const {
  return {};
}

}  // namespace update_client
