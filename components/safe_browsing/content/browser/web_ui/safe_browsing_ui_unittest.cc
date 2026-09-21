// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_content_ui_handler.h"
#include "components/safe_browsing/core/browser/db/v5_get_hash_protocol_manager.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingUITest : public testing::Test {
 public:
  SafeBrowsingUITest() = default;

  void SetUp() override {
    os_crypt_async_ = os_crypt_async::GetTestOSCryptAsyncForTesting(
        /*is_sync_for_unittests=*/true);
  }

  int SetMemberInt(int member_int) {
    member_int_ = member_int;
    return member_int_;
  }

  SafeBrowsingContentUIHandler* RegisterNewHandler() {
    auto handler_unique = std::make_unique<SafeBrowsingContentUIHandler>(
        &browser_context_, nullptr, os_crypt_async_.get());

    SafeBrowsingContentUIHandler* handler = handler_unique.get();
    handler->SetWebUIForTesting(&web_ui_);
    // Calling AllowJavascript will register the handler as a web UI instance
    // for WebUIContentInfoSingleton::GetInstance(). We do this instead of
    // registering the instance directly because otherwise, the first
    // SafeBrowsingContentUIHandler call to AllowJavascript will re-register the
    // instance.
    handler->AllowJavascriptForTesting();

    web_ui_.AddMessageHandler(std::move(handler_unique));
    return handler;
  }

  void UnregisterHandler(SafeBrowsingContentUIHandler* handler) {
    WebUIContentInfoSingleton::GetInstance()->UnregisterWebUIInstance(
        handler->event_observer());
  }

  V5GetHashProtocolManager::V5GetHashLookup CreateV5GetHashLookup() {
    V5GetHashProtocolManager::V5GetHashLookup log;
    log.urls = {GURL("https://example.com/test1"),
                GURL("https://example.com/test2")};
    log.check_type = ClientCallbackType::CHECK_BROWSE_URL;
    log.local_threat_types = {SBThreatType::SB_THREAT_TYPE_URL_PHISHING};
    log.request_proto.add_hash_prefixes("test");
    log.response_code = 200;
    log.net_error = 0;
    log.severest_threat_type = SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    log.metadata.subresource_filter_match[SubresourceFilterType::ABUSIVE] =
        SubresourceFilterLevel::ENFORCE;
    log.metadata.subresource_filter_match[SubresourceFilterType::BETTER_ADS] =
        SubresourceFilterLevel::WARN;

    V5::SearchHashesResponse response;
    V5::Duration* cache_duration = response.mutable_cache_duration();
    cache_duration->set_seconds(300);
    cache_duration->set_nanos(0);
    V5::FullHash* full_hash = response.add_full_hashes();
    full_hash->set_full_hash("test_hash");
    full_hash->add_full_hash_details()->set_threat_type(
        V5::ThreatType::SOCIAL_ENGINEERING);
    log.response_proto = response;
    return log;
  }

 protected:
  int member_int_;
  std::unique_ptr<os_crypt_async::OSCryptAsync> os_crypt_async_;
  content::TestWebUI web_ui_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestBrowserContext browser_context_;
};

TEST_F(SafeBrowsingUITest, CRSBLOGDoesNotEvaluateWhenNoListeners) {
  member_int_ = 0;

  // Start with no listeners, so SetMemberInt() should not be evaluated.
  CRSBLOG << SetMemberInt(1);
  EXPECT_EQ(member_int_, 0);

  // Register a listener, so SetMemberInt() will be evaluated.
  SafeBrowsingContentUIHandler* handler = RegisterNewHandler();

  CRSBLOG << SetMemberInt(1);
  EXPECT_EQ(member_int_, 1);

  UnregisterHandler(handler);
}

TEST_F(SafeBrowsingUITest, TestHPRTLookups) {
  SafeBrowsingContentUIHandler* handler = RegisterNewHandler();
  ASSERT_EQ(0u, web_ui_.call_data().size());

  // Create request.
  std::unique_ptr<V5::SearchHashesRequest> inner_request =
      std::make_unique<V5::SearchHashesRequest>();
  inner_request->add_hash_prefixes("hash_prefix_1");
  inner_request->add_hash_prefixes("hash_prefix_2");
  std::string relay_url_spec = "testing_relay_url_spec";
  std::string ohttp_key = "testing_ohttp_key";
  // Add request to pings.
  std::optional<int> token =
      WebUIContentInfoSingleton::GetInstance()->AddToHPRTLookupPings(
          inner_request.get(), relay_url_spec, ohttp_key);
  // Validate request call_data.
  ASSERT_TRUE(token.has_value());
  ASSERT_EQ(1u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[0]->arg1()->GetString(),
            "hprt-lookup-pings-update");
  const base::ListValue& request_data =
      web_ui_.call_data()[0]->arg2()->GetList();
  EXPECT_EQ(request_data[0].GetInt(), token.value());
  EXPECT_EQ(base::test::ParseJson(request_data[1].GetString()),
            base::test::ParseJson(R"!({
   "inner_request": {
      "hash_prefixes (base64)": [ "aGFzaF9wcmVmaXhfMQ==", "aGFzaF9wcmVmaXhfMg==" ]
   },
   "ohttp_public_key (base64)": "dGVzdGluZ19vaHR0cF9rZXk=",
   "relay_url": "testing_relay_url_spec"
})!"));

  // Create response.
  std::unique_ptr<V5::SearchHashesResponse> response =
      std::make_unique<V5::SearchHashesResponse>();
  V5::Duration* cache_duration = response->mutable_cache_duration();
  cache_duration->set_seconds(123);
  cache_duration->set_nanos(30);
  // Full hash 1
  V5::FullHash* full_hash_1 = response->add_full_hashes();
  full_hash_1->set_full_hash("full_hash_1");
  V5::FullHash::FullHashDetail* full_hash_detail_1 =
      full_hash_1->add_full_hash_details();
  full_hash_detail_1->set_threat_type(
      ::safe_browsing::V5::ThreatType::SOCIAL_ENGINEERING);
  V5::FullHash::FullHashDetail* full_hash_detail_2 =
      full_hash_1->add_full_hash_details();
  full_hash_detail_2->set_threat_type(::safe_browsing::V5::ThreatType::MALWARE);
  // Full hash 2
  V5::FullHash* full_hash_2 = response->add_full_hashes();
  full_hash_2->set_full_hash("full_hash_2");
  V5::FullHash::FullHashDetail* full_hash_detail_3 =
      full_hash_2->add_full_hash_details();
  full_hash_detail_3->set_threat_type(::safe_browsing::V5::ThreatType::MALWARE);
  full_hash_detail_3->add_attributes(
      ::safe_browsing::V5::ThreatAttribute::CANARY);
  full_hash_detail_3->add_attributes(
      ::safe_browsing::V5::ThreatAttribute::FRAME_ONLY);
  // Add response to pings.
  WebUIContentInfoSingleton::GetInstance()->AddToHPRTLookupResponses(
      token.value(), response.get());
  // Validate response call_data.
  ASSERT_EQ(2u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[1]->arg1()->GetString(),
            "hprt-lookup-responses-update");
  const base::ListValue& response_data =
      web_ui_.call_data()[1]->arg2()->GetList();
  EXPECT_EQ(response_data[0].GetInt(), token.value());
  EXPECT_EQ(base::test::ParseJson(response_data[1].GetString()),
            base::test::ParseJson(R"!({
   "cache_duration": {
      "nanos": 30,
      "seconds": "123"
   },
   "full_hashes": [ {
      "full_hash": "ZnVsbF9oYXNoXzE=",
      "full_hash_details": [ {
         "threat_type": "SOCIAL_ENGINEERING"
      }, {
         "threat_type": "MALWARE"
      } ]
   }, {
      "full_hash": "ZnVsbF9oYXNoXzI=",
      "full_hash_details": [ {
         "attributes": [ "CANARY", "FRAME_ONLY" ],
         "threat_type": "MALWARE"
      } ]
   } ]
}
)!"));

  // Simulate JS calling getHPRTLookupPings and validate call_data.
  base::ListValue call_args;
  call_args.Append("dummy-callback-id-1");
  web_ui_.HandleReceivedMessage("getHPRTLookupPings", call_args);
  ASSERT_EQ(3u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[2]->arg1()->GetString(), "dummy-callback-id-1");
  EXPECT_EQ(web_ui_.call_data()[2]->arg2()->GetBool(), true);
  const base::ListValue& request_pings =
      web_ui_.call_data()[2]->arg3()->GetList();
  ASSERT_EQ(request_pings.size(), 1u);
  EXPECT_EQ(request_pings[0], request_data);

  // Simulate JS calling getHPRTLookupResponses and validate call_data.
  base::ListValue call_args2;
  call_args2.Append("dummy-callback-id-2");
  web_ui_.HandleReceivedMessage("getHPRTLookupResponses", call_args2);
  ASSERT_EQ(4u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[3]->arg1()->GetString(), "dummy-callback-id-2");
  EXPECT_EQ(web_ui_.call_data()[3]->arg2()->GetBool(), true);
  const base::ListValue& response_pings =
      web_ui_.call_data()[3]->arg3()->GetList();
  ASSERT_EQ(response_pings.size(), 1u);
  EXPECT_EQ(response_pings[0], response_data);

  UnregisterHandler(handler);
}

TEST_F(SafeBrowsingUITest, TestV5GetHashLookups) {
  base::test::ScopedFeatureList scoped_feature_list(kLocalListsUseSBv5WebUI);
  SafeBrowsingContentUIHandler* handler = RegisterNewHandler();
  ASSERT_EQ(0u, web_ui_.call_data().size());

  WebUIContentInfoSingleton::GetInstance()->AddToV5GetHashLookups(
      CreateV5GetHashLookup());

  ASSERT_EQ(1u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[0]->arg1()->GetString(),
            "v5-get-hash-lookup-update");
  const base::DictValue& lookup_data =
      web_ui_.call_data()[0]->arg2()->GetDict();

  const base::ListValue* request_key_values =
      lookup_data.FindList("requestKeyValues");
  ASSERT_TRUE(request_key_values);
  EXPECT_EQ(*request_key_values, base::test::ParseJson(R"!([
    {"key": "Check type", "value": "CHECK_BROWSE_URL"},
    {"key": "Local threat types", "value": "URL_PHISHING"},
    {"key": "URLs", "value": "https://example.com/test1, https://example.com/test2"}
  ])!")
                                     .GetList());

  const std::string* request_search_hashes_json =
      lookup_data.FindString("requestSearchHashesJson");
  ASSERT_TRUE(request_search_hashes_json);
  EXPECT_EQ(base::test::ParseJson(*request_search_hashes_json),
            base::test::ParseJson(R"!({
    "hash_prefixes": [
      "dGVzdA=="
    ]
  })!"));

  const base::ListValue* response_key_values =
      lookup_data.FindList("responseKeyValues");
  ASSERT_TRUE(response_key_values);
  EXPECT_EQ(*response_key_values, base::test::ParseJson(R"!([
    {"key": "Response code", "value": "200"},
    {"key": "Net error", "value": "net::OK"},
    {"key": "Severest threat type", "value": "URL_PHISHING"},
    {"key": "Subresource filter", "value": "ABUSIVE (ENFORCE), BETTER_ADS (WARN)"}
  ])!")
                                      .GetList());

  const std::string* response_search_hashes_json =
      lookup_data.FindString("responseSearchHashesJson");
  ASSERT_TRUE(response_search_hashes_json);
  EXPECT_EQ(base::test::ParseJson(*response_search_hashes_json),
            base::test::ParseJson(R"!({
    "cache_duration": {
      "nanos": 0,
      "seconds": "300"
    },
    "full_hashes": [ {
      "full_hash": "dGVzdF9oYXNo",
      "full_hash_details": [ {
        "threat_type": "SOCIAL_ENGINEERING"
      } ]
    } ]
  })!"));

  // Log a second lookup with a network error and no URLs (e.g. extension
  // check).
  V5GetHashProtocolManager::V5GetHashLookup failed_log;
  failed_log.check_type = ClientCallbackType::CHECK_EXTENSION_IDS;
  failed_log.local_threat_types = {SBThreatType::SB_THREAT_TYPE_EXTENSION};
  failed_log.request_proto.add_hash_prefixes("fail");
  failed_log.net_error = net::ERR_FAILED;

  WebUIContentInfoSingleton::GetInstance()->AddToV5GetHashLookups(failed_log);

  ASSERT_EQ(2u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[1]->arg1()->GetString(),
            "v5-get-hash-lookup-update");
  const base::DictValue& failed_lookup_data =
      web_ui_.call_data()[1]->arg2()->GetDict();

  const base::ListValue* failed_request_key_values =
      failed_lookup_data.FindList("requestKeyValues");
  ASSERT_TRUE(failed_request_key_values);
  EXPECT_EQ(*failed_request_key_values, base::test::ParseJson(R"!([
    {"key": "Check type", "value": "CHECK_EXTENSION_IDS"},
    {"key": "Local threat types", "value": "EXTENSION"}
  ])!")
                                            .GetList());

  const std::string* failed_request_search_hashes_json =
      failed_lookup_data.FindString("requestSearchHashesJson");
  ASSERT_TRUE(failed_request_search_hashes_json);
  EXPECT_EQ(base::test::ParseJson(*failed_request_search_hashes_json),
            base::test::ParseJson(R"!({
    "hash_prefixes": [
      "ZmFpbA=="
    ]
  })!"));

  const base::ListValue* failed_response_key_values =
      failed_lookup_data.FindList("responseKeyValues");
  ASSERT_TRUE(failed_response_key_values);
  EXPECT_EQ(*failed_response_key_values, base::test::ParseJson(R"!([
    {"key": "Response code", "value": "0"},
    {"key": "Net error", "value": "net::ERR_FAILED"},
    {"key": "Severest threat type", "value": "SAFE"}
  ])!")
                                             .GetList());

  EXPECT_FALSE(failed_lookup_data.contains("responseSearchHashesJson"));

  // Simulate JS calling getV5GetHashLookups and validate call_data.
  base::ListValue call_args;
  call_args.Append("dummy-callback-id-1");
  web_ui_.HandleReceivedMessage("getV5GetHashLookups", call_args);
  ASSERT_EQ(3u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[2]->arg1()->GetString(), "dummy-callback-id-1");
  EXPECT_EQ(web_ui_.call_data()[2]->arg2()->GetBool(), true);
  const base::ListValue& lookups = web_ui_.call_data()[2]->arg3()->GetList();
  ASSERT_EQ(lookups.size(), 2u);
  EXPECT_EQ(lookups[0].GetDict(), lookup_data);
  EXPECT_EQ(lookups[1].GetDict(), failed_lookup_data);

  UnregisterHandler(handler);
}

TEST_F(SafeBrowsingUITest, TestV5GetHashLookups_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kLocalListsUseSBv5WebUI);
  SafeBrowsingContentUIHandler* handler = RegisterNewHandler();
  ASSERT_EQ(0u, web_ui_.call_data().size());

  WebUIContentInfoSingleton::GetInstance()->AddToV5GetHashLookups(
      CreateV5GetHashLookup());

  EXPECT_EQ(0u, web_ui_.call_data().size());

  UnregisterHandler(handler);
}

}  // namespace safe_browsing
