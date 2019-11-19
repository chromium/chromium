// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/web_resource/resource_request_allowed_notifier.h"
#include "components/web_resource/web_resource_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const std::string kTestUrl = "http://www.test.com";
const std::string kCacheUpdatePath = "cache_update_path";
std::string error_message_;
}  // namespace

namespace web_resource {

class TestResourceRequestAllowedNotifier
    : public ResourceRequestAllowedNotifier {
 public:
  TestResourceRequestAllowedNotifier(
      PrefService* prefs,
      const char* disable_network_switch,
      network::NetworkConnectionTracker* network_connection_tracker)
      : ResourceRequestAllowedNotifier(
            prefs,
            disable_network_switch,
            base::BindOnce(
                [](network::NetworkConnectionTracker* tracker) {
                  return tracker;
                },
                network_connection_tracker)) {}

  ResourceRequestAllowedNotifier::State GetResourceRequestsAllowedState()
      override {
    return state_;
  }

  void SetState(ResourceRequestAllowedNotifier::State state) { state_ = state; }

  void NotifyState(ResourceRequestAllowedNotifier::State state) {
    SetState(state);
    SetObserverRequestedForTesting(true);
    MaybeNotifyObserver();
  }

 private:
  ResourceRequestAllowedNotifier::State state_;
};

class TestWebResourceService : public WebResourceService {
 public:
  TestWebResourceService(
      PrefService* prefs,
      const GURL& web_resource_server,
      const std::string& application_locale,
      const char* last_update_time_pref_name,
      int start_fetch_delay_ms,
      int cache_update_delay_ms,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const char* disable_network_switch,
      network::NetworkConnectionTracker* network_connection_tracker)
      : WebResourceService(prefs,
                           web_resource_server,
                           application_locale,
                           last_update_time_pref_name,
                           start_fetch_delay_ms,
                           cache_update_delay_ms,
                           url_loader_factory,
                           disable_network_switch,
                           TRAFFIC_ANNOTATION_FOR_TESTS,
                           base::BindOnce(
                               [](network::NetworkConnectionTracker* tracker) {
                                 return tracker;
                               },
                               network_connection_tracker)) {}

  void Unpack(const base::DictionaryValue& parsed_json) override {}
};

class WebResourceServiceTest : public testing::Test {
 public:
  void SetUp() override {
    test_shared_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    local_state_.reset(new TestingPrefServiceSimple());
    local_state_->registry()->RegisterStringPref(kCacheUpdatePath, "0");
    test_web_resource_service_.reset(new TestWebResourceService(
        local_state_.get(), GURL(kTestUrl), "", kCacheUpdatePath.c_str(), 100,
        5000, test_shared_loader_factory_, nullptr,
        network::TestNetworkConnectionTracker::GetInstance()));
    error_message_ = "";
    TestResourceRequestAllowedNotifier* notifier =
        new TestResourceRequestAllowedNotifier(
            local_state_.get(), nullptr,
            network::TestNetworkConnectionTracker::GetInstance());
    notifier->SetState(ResourceRequestAllowedNotifier::ALLOWED);
    test_web_resource_service_->SetResourceRequestAllowedNotifier(
        std::unique_ptr<ResourceRequestAllowedNotifier>(notifier));
  }

  TestResourceRequestAllowedNotifier* resource_notifier() {
    return static_cast<TestResourceRequestAllowedNotifier*>(
        test_web_resource_service_->resource_request_allowed_notifier_.get());
  }

  bool GetFetchScheduled() {
    return test_web_resource_service_->GetFetchScheduled();
  }

  void CallScheduleFetch(int64_t delay_ms) {
    return test_web_resource_service_->ScheduleFetch(delay_ms);
  }

  WebResourceService* web_resource_service() {
    return test_web_resource_service_.get();
  }

  void CallStartFetch() { test_web_resource_service_->StartFetch(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
  std::unique_ptr<TestWebResourceService> test_web_resource_service_;
};

TEST_F(WebResourceServiceTest, FetchScheduledAfterStartDelayTest) {
  web_resource_service()->StartAfterDelay();
  EXPECT_TRUE(GetFetchScheduled());
}

TEST_F(WebResourceServiceTest, FetchScheduledOnScheduleFetchTest) {
  web_resource_service()->StartAfterDelay();
  resource_notifier()->NotifyState(ResourceRequestAllowedNotifier::ALLOWED);
  EXPECT_TRUE(GetFetchScheduled());
}

TEST_F(WebResourceServiceTest, FetchScheduledOnStartFetchTest) {
  resource_notifier()->NotifyState(
      ResourceRequestAllowedNotifier::DISALLOWED_NETWORK_DOWN);
  CallStartFetch();
  EXPECT_FALSE(GetFetchScheduled());
  resource_notifier()->NotifyState(ResourceRequestAllowedNotifier::ALLOWED);
  EXPECT_TRUE(GetFetchScheduled());
}

}  // namespace web_resource
