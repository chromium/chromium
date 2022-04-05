// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/capabilities_service_impl.h"

#include <set>
#include <vector>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/autofill_assistant/browser/public/mock_autofill_assistant.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::NiceMock;
using CapabilitiesInfo =
    autofill_assistant::AutofillAssistant::CapabilitiesInfo;

constexpr uint32_t kHashPrefixSize = 15;
constexpr uint64_t kExampleDotComHash = 2170UL;
constexpr uint64_t kTestDotComHash = 17534UL;
constexpr uint64_t kDummyurlDotComHash = 15654UL;

constexpr char kPasswordChangeIntent[] = "PASSWORD_CHANGE";

class CapabilitiesServiceImplTest : public ::testing::Test {
 public:
  CapabilitiesServiceImplTest() {
    auto autofill_assistant =
        std::make_unique<NiceMock<autofill_assistant::MockAutofillAssistant>>();
    mock_autofill_assistant_ = autofill_assistant.get();

    service_ = std::make_unique<CapabilitiesServiceImpl>(
        std::move(autofill_assistant));
  }
  ~CapabilitiesServiceImplTest() override = default;

 protected:
  raw_ptr<NiceMock<autofill_assistant::MockAutofillAssistant>>
      mock_autofill_assistant_;
  std::unique_ptr<CapabilitiesServiceImpl> service_;
};

TEST_F(CapabilitiesServiceImplTest, FetchCapabilitiesEmptyResponse) {
  base::HistogramTester histogram_tester;
  std::vector<url::Origin> origins = {
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://test.com"))};

  EXPECT_CALL(*mock_autofill_assistant_,
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize,
                  std::vector<uint64_t>{kExampleDotComHash, kTestDotComHash},
                  kPasswordChangeIntent, _))
      .WillOnce(
          RunOnceCallback<3>(net::HTTP_OK, std::vector<CapabilitiesInfo>()));

  base::MockCallback<password_manager::CapabilitiesService::ResponseCallback>
      mock_response_callback;
  EXPECT_CALL(mock_response_callback, Run(std::set<url::Origin>()));

  service_->QueryPasswordChangeScriptAvailability(origins,
                                                  mock_response_callback.Get());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CapabilitiesService.HttpResponseCode",
      net::HttpStatusCode::HTTP_OK, 1u);
}

TEST_F(CapabilitiesServiceImplTest, FetchCapabilitiesEmptyRequest) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_autofill_assistant_, GetCapabilitiesByHashPrefix).Times(0);
  base::MockCallback<password_manager::CapabilitiesService::ResponseCallback>
      mock_response_callback;
  EXPECT_CALL(mock_response_callback, Run(std::set<url::Origin>()));

  service_->QueryPasswordChangeScriptAvailability({},
                                                  mock_response_callback.Get());
  histogram_tester.ExpectTotalCount(
      "PasswordManager.CapabilitiesService.HttpResponseCode", 0u);
}

TEST_F(CapabilitiesServiceImplTest, BackendRequestFailed) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*mock_autofill_assistant_, GetCapabilitiesByHashPrefix)
      .WillOnce(RunOnceCallback<3>(net::HTTP_FORBIDDEN,
                                   std::vector<CapabilitiesInfo>()));

  base::MockCallback<password_manager::CapabilitiesService::ResponseCallback>
      mock_response_callback;
  EXPECT_CALL(mock_response_callback, Run(std::set<url::Origin>()));

  std::vector<url::Origin> origins = {
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://test.com"))};

  service_->QueryPasswordChangeScriptAvailability(origins,
                                                  mock_response_callback.Get());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CapabilitiesService.HttpResponseCode",
      net::HTTP_FORBIDDEN, 1u);
}

TEST_F(CapabilitiesServiceImplTest, FetchCapabilitiesSuccess) {
  base::HistogramTester histogram_tester;
  std::vector<CapabilitiesInfo> capabilities_reponse = {
      CapabilitiesInfo{"https://foo.test.com", {}},
      CapabilitiesInfo{"https://bar.test.com", {}},
      CapabilitiesInfo{"https://example.com", {}},
      CapabilitiesInfo{"https://test.com", {}},
      CapabilitiesInfo{"https://dummyurl.com",
                       {{{"EXPERIMENT_IDS", "3345172"}}}},
  };

  EXPECT_CALL(*mock_autofill_assistant_,
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize,
                  std::vector<uint64_t>{kExampleDotComHash, kTestDotComHash,
                                        kDummyurlDotComHash},
                  kPasswordChangeIntent, _))
      .WillOnce(RunOnceCallback<3>(net::HTTP_OK, capabilities_reponse));

  base::MockCallback<password_manager::CapabilitiesService::ResponseCallback>
      mock_response_callback;

  EXPECT_CALL(mock_response_callback,
              Run(std::set<url::Origin>{
                  url::Origin::Create(GURL("https://example.com")),
                  url::Origin::Create(GURL("https://test.com")),
              }));

  std::vector<url::Origin> origins = {
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://dummyurl.com")),
  };

  service_->QueryPasswordChangeScriptAvailability(origins,
                                                  mock_response_callback.Get());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CapabilitiesService.HttpResponseCode",
      net::HttpStatusCode::HTTP_OK, 1u);
}

TEST_F(CapabilitiesServiceImplTest, FetchCapabilitiesClientInLiveExperiment) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      password_manager::features::kPasswordDomainCapabilitiesFetching,
      {{"live_experiment", "true"}});

  std::vector<CapabilitiesInfo> capabilities_reponse = {
      CapabilitiesInfo{"https://foo.test.com", {}},
      CapabilitiesInfo{"https://bar.test.com", {}},
      CapabilitiesInfo{"https://example.com", {}},
      CapabilitiesInfo{"https://test.com", {}},
      CapabilitiesInfo{"https://dummyurl.com",
                       {{{"EXPERIMENT_IDS", "3345172"}}}},
  };

  EXPECT_CALL(*mock_autofill_assistant_,
              GetCapabilitiesByHashPrefix(
                  kHashPrefixSize,
                  std::vector<uint64_t>{kExampleDotComHash, kTestDotComHash,
                                        kDummyurlDotComHash},
                  kPasswordChangeIntent, _))
      .WillOnce(RunOnceCallback<3>(net::HTTP_OK, capabilities_reponse));

  base::MockCallback<password_manager::CapabilitiesService::ResponseCallback>
      mock_response_callback;
  EXPECT_CALL(mock_response_callback,
              Run(std::set<url::Origin>{
                  url::Origin::Create(GURL("https://example.com")),
                  url::Origin::Create(GURL("https://test.com")),
                  url::Origin::Create(GURL("https://dummyurl.com")),
              }));

  std::vector<url::Origin> origins = {
      url::Origin::Create(GURL("https://example.com")),
      url::Origin::Create(GURL("https://test.com")),
      url::Origin::Create(GURL("https://dummyurl.com")),
  };

  service_->QueryPasswordChangeScriptAvailability(origins,
                                                  mock_response_callback.Get());
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.CapabilitiesService.HttpResponseCode",
      net::HttpStatusCode::HTTP_OK, 1u);
}

}  // namespace
}  // namespace password_manager
