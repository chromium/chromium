// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "base/test/values_test_util.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SafeBrowsingUITest : public testing::Test {
 public:
  SafeBrowsingUITest() {}

  void SetUp() override {}

  int SetMemberInt(int member_int) {
    member_int_ = member_int;
    return member_int_;
  }

  SafeBrowsingUIHandler* RegisterNewHandler() {
    auto handler_unique =
        std::make_unique<SafeBrowsingUIHandler>(&browser_context_, nullptr);

    SafeBrowsingUIHandler* handler = handler_unique.get();
    handler->SetWebUIForTesting(&web_ui_);
    // Calling AllowJavascript will register the handler as a web UI instance
    // for WebUIInfoSingleton::GetInstance(). We do this instead of registering
    // the instance directly because otherwise, the first SafeBrowsingUIHandler
    // call to AllowJavascript will re-register the instance.
    handler->AllowJavascriptForTesting();

    web_ui_.AddMessageHandler(std::move(handler_unique));
    return handler;
  }

  void UnregisterHandler(SafeBrowsingUIHandler* handler) {
    WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(handler);
  }

 protected:
  int member_int_;
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
  SafeBrowsingUIHandler* handler = RegisterNewHandler();

  CRSBLOG << SetMemberInt(1);
  EXPECT_EQ(member_int_, 1);

  UnregisterHandler(handler);
}

TEST_F(SafeBrowsingUITest, TestHPRTLookups) {
  SafeBrowsingUIHandler* handler = RegisterNewHandler();
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
      WebUIInfoSingleton::GetInstance()->AddToHPRTLookupPings(
          inner_request.get(), relay_url_spec, ohttp_key);
  // Validate request call_data.
  ASSERT_TRUE(token.has_value());
  ASSERT_EQ(1u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[0]->arg1()->GetString(),
            "hprt-lookup-pings-update");
  const base::Value::List& request_data =
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
  WebUIInfoSingleton::GetInstance()->AddToHPRTLookupResponses(token.value(),
                                                              response.get());
  // Validate response call_data.
  ASSERT_EQ(2u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[1]->arg1()->GetString(),
            "hprt-lookup-responses-update");
  const base::Value::List& response_data =
      web_ui_.call_data()[1]->arg2()->GetList();
  EXPECT_EQ(response_data[0].GetInt(), token.value());
  EXPECT_EQ(base::test::ParseJson(response_data[1].GetString()),
            base::test::ParseJson(R"!({
   "cache_duration": {
      "nanos": 30.0,
      "seconds": 123.0
   },
   "full_hashes": [ {
      "full_hash (base64)": "ZnVsbF9oYXNoXzE=",
      "full_hash_details": [ {
         "attributes": [  ],
         "threat_type": "SOCIAL_ENGINEERING"
      }, {
         "attributes": [  ],
         "threat_type": "MALWARE"
      } ]
   }, {
      "full_hash (base64)": "ZnVsbF9oYXNoXzI=",
      "full_hash_details": [ {
         "attributes": [ "CANARY", "FRAME_ONLY" ],
         "threat_type": "MALWARE"
      } ]
   } ]
}
)!"));

  // Simulate JS calling getHPRTLookupPings and validate call_data.
  base::Value::List call_args;
  call_args.Append("dummy-callback-id-1");
  web_ui_.HandleReceivedMessage("getHPRTLookupPings", call_args);
  ASSERT_EQ(3u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[2]->arg1()->GetString(), "dummy-callback-id-1");
  EXPECT_EQ(web_ui_.call_data()[2]->arg2()->GetBool(), true);
  const base::Value::List& request_pings =
      web_ui_.call_data()[2]->arg3()->GetList();
  ASSERT_EQ(request_pings.size(), 1u);
  EXPECT_EQ(request_pings[0], request_data);

  // Simulate JS calling getHPRTLookupResponses and validate call_data.
  base::Value::List call_args2;
  call_args2.Append("dummy-callback-id-2");
  web_ui_.HandleReceivedMessage("getHPRTLookupResponses", call_args2);
  ASSERT_EQ(4u, web_ui_.call_data().size());
  EXPECT_EQ(web_ui_.call_data()[3]->arg1()->GetString(), "dummy-callback-id-2");
  EXPECT_EQ(web_ui_.call_data()[3]->arg2()->GetBool(), true);
  const base::Value::List& response_pings =
      web_ui_.call_data()[3]->arg3()->GetList();
  ASSERT_EQ(response_pings.size(), 1u);
  EXPECT_EQ(response_pings[0], response_data);

  UnregisterHandler(handler);
}

}  // namespace safe_browsing
