// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/aim_eligibility_service.h"

#include <memory>
#include <string>
#include <utility>

#include "base/run_loop.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/omnibox_proto/aim_eligibility_response.pb.h"
#include "url/gurl.h"

using testing::ReturnRef;

namespace content {
class WebContents;
}  // namespace content

namespace {
// A mock AimEligibilityService that provides a mock response for member
// functions to use.
class MockAimEligibilityServiceForInterception : public AimEligibilityService {
 public:
  MockAimEligibilityServiceForInterception(PrefService& pref_service)
      : AimEligibilityService(pref_service, nullptr, nullptr, nullptr, false) {}
  ~MockAimEligibilityServiceForInterception() override = default;

  MOCK_METHOD(const omnibox::AimEligibilityResponse&,
              GetMostRecentResponse,
              (),
              (const, override));
  MOCK_METHOD(std::string, GetCountryCode, (), (const, override));
  MOCK_METHOD(std::string, GetLocale, (), (const, override));

  void SetAimEligibilityResponse(omnibox::AimEligibilityResponse response) {
    eligibility_response_ = std::move(response);
    ON_CALL(*this, GetMostRecentResponse())
        .WillByDefault(ReturnRef(eligibility_response_));
  }

 private:
  omnibox::AimEligibilityResponse eligibility_response_;
};

omnibox::AimEligibilityResponse::QueryParam CreateRequiredParam(
    const std::string& key,
    const std::string& value) {
  omnibox::AimEligibilityResponse::QueryParam param;
  param.set_key(key);
  param.set_value(value);
  return param;
}

}  // namespace

class AimEligibilityServiceTest : public testing::Test {
 public:
  explicit AimEligibilityServiceTest() {}

  void SetUp() override {
    aim_eligibility_service_ =
        std::make_unique<MockAimEligibilityServiceForInterception>(prefs_);
  }

  void TearDown() override { aim_eligibility_service_ = nullptr; }

 protected:
  TestingPrefServiceSimple prefs_;
  std::unique_ptr<MockAimEligibilityServiceForInterception>
      aim_eligibility_service_;
};

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_Match) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?a=1&b=2");

  EXPECT_TRUE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_MissingParam) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?a=1");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_NoParams) {
  omnibox::AimEligibilityResponse response;
  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule;

  rule.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_MultipleRules) {
  omnibox::AimEligibilityResponse response;

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule1;
  rule1.mutable_required_params()->Add(CreateRequiredParam("a", "1"));
  rule1.mutable_required_params()->Add(CreateRequiredParam("b", "2"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule1));

  omnibox::AimEligibilityResponse::AimDetectionUrlRule rule2;
  rule2.mutable_required_params()->Add(CreateRequiredParam("c", "1"));
  response.mutable_aim_detection_url_rule()->Add(std::move(rule2));

  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?c=1");

  EXPECT_TRUE(aim_eligibility_service_->HasAimUrlParams(url));
}

TEST_F(AimEligibilityServiceTest, UrlInterceptRules_NoRules) {
  omnibox::AimEligibilityResponse response;
  aim_eligibility_service_->SetAimEligibilityResponse(std::move(response));

  GURL url("https://google.com?c=1");

  EXPECT_FALSE(aim_eligibility_service_->HasAimUrlParams(url));
}
