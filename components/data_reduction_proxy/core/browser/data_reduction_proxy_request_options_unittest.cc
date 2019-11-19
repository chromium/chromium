// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/proxy_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kChromeProxyHeader[] = "chrome-proxy";

const char kVersion[] = "0.1.2.3";
const char kExpectedBuild[] = "2";
const char kExpectedPatch[] = "3";
const char kPageId[] = "1";
const uint64_t kPageIdValue = 1;

const char kTestKey2[] = "test-key2";
const char kPageId2[] = "f";

const char kSecureSession[] = "TestSecureSessionKey";
}  // namespace


namespace data_reduction_proxy {
namespace {

#if defined(OS_ANDROID)
const Client kClient = Client::CHROME_ANDROID;
const char kClientStr[] = "android";
#elif defined(OS_IOS)
const Client kClient = Client::CHROME_IOS;
const char kClientStr[] = "ios";
#elif defined(OS_MACOSX)
const Client kClient = Client::CHROME_MAC;
const char kClientStr[] = "mac";
#elif defined(OS_CHROMEOS)
const Client kClient = Client::CHROME_CHROMEOS;
const char kClientStr[] = "chromeos";
#elif defined(OS_LINUX)
const Client kClient = Client::CHROME_LINUX;
const char kClientStr[] = "linux";
#elif defined(OS_WIN)
const Client kClient = Client::CHROME_WINDOWS;
const char kClientStr[] = "win";
#elif defined(OS_FREEBSD)
const Client kClient = Client::CHROME_FREEBSD;
const char kClientStr[] = "freebsd";
#elif defined(OS_OPENBSD)
const Client kClient = Client::CHROME_OPENBSD;
const char kClientStr[] = "openbsd";
#elif defined(OS_SOLARIS)
const Client kClient = Client::CHROME_SOLARIS;
const char kClientStr[] = "solaris";
#elif defined(OS_QNX)
const Client kClient = Client::CHROME_QNX;
const char kClientStr[] = "qnx";
#else
const Client kClient = Client::UNKNOWN;
const char kClientStr[] = "";
#endif

void SetHeaderExpectations(const std::string& secure_session,
                           const std::string& client,
                           const std::string& build,
                           const std::string& patch,
                           const std::string& page_id,
                           const std::string& server_experiments,
                           std::string* expected_header) {
  std::vector<std::string> expected_options;
  if (!secure_session.empty()) {
    expected_options.push_back(std::string(kSecureSessionHeaderOption) + "=" +
                               secure_session);
  }
  if (!client.empty()) {
    expected_options.push_back(
        std::string(kClientHeaderOption) + "=" + client);
  }
  EXPECT_FALSE(build.empty());
  expected_options.push_back(std::string(kBuildNumberHeaderOption) + "=" +
                             build);
  EXPECT_FALSE(patch.empty());
  expected_options.push_back(std::string(kPatchNumberHeaderOption) + "=" +
                             patch);

  if (!server_experiments.empty()) {
    expected_options.push_back(
        std::string(params::GetDataSaverServerExperimentsOptionName()) + "=" +
        server_experiments);
  }

  EXPECT_FALSE(page_id.empty());
  expected_options.push_back("pid=" + page_id);

  if (!expected_options.empty())
    *expected_header = base::JoinString(expected_options, ", ");
}

}  // namespace

class DataReductionProxyRequestOptionsTest : public testing::Test {
 public:
  DataReductionProxyRequestOptionsTest() {
    test_context_ =
        DataReductionProxyTestContext::Builder()
            .Build();
  }

  void CreateRequestOptions(const std::string& version) {
    request_options_.reset(new TestDataReductionProxyRequestOptions(
        kClient, version, test_context_->config()));
    request_options_->Init();
  }

  void CreateRequestOptionsWithCallback(const std::string& version) {
    CreateRequestOptions(version);
    request_options_->SetUpdateHeaderCallback(base::BindRepeating(
        &DataReductionProxyRequestOptionsTest::UpdateHeaderCallback,
        base::Unretained(this)));
  }

  void UpdateHeaderCallback(const net::HttpRequestHeaders& headers) {
    callback_headers_ = headers;
  }

  TestDataReductionProxyParams* params() {
    return test_context_->config()->test_params();
  }

  TestDataReductionProxyRequestOptions* request_options() {
    return request_options_.get();
  }

  net::HttpRequestHeaders callback_headers() { return callback_headers_; }

  void VerifyExpectedHeader(const std::string& expected_header,
                            uint64_t page_id) {
    test_context_->RunUntilIdle();
    net::HttpRequestHeaders headers;
    request_options_->AddRequestHeader(&headers, page_id);
    if (expected_header.empty()) {
      EXPECT_FALSE(headers.HasHeader(kChromeProxyHeader));
      return;
    }
    EXPECT_TRUE(headers.HasHeader(kChromeProxyHeader));
    std::string header_value;
    headers.GetHeader(kChromeProxyHeader, &header_value);
    EXPECT_EQ(expected_header, header_value);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  std::unique_ptr<TestDataReductionProxyRequestOptions> request_options_;
  std::unique_ptr<DataReductionProxyTestContext> test_context_;
  net::HttpRequestHeaders callback_headers_;
};

TEST_F(DataReductionProxyRequestOptionsTest, Authorization) {
  std::string expected_header;
  SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, std::string(),
                        &expected_header);

  std::string expected_header2;
  SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId2, std::string(),
                        &expected_header2);

  CreateRequestOptions(kVersion);
  test_context_->RunUntilIdle();

  // Now set a key.
  request_options()->SetKeyForTesting(kTestKey2);

  // Write headers.
  VerifyExpectedHeader(expected_header, kPageIdValue);
}

TEST_F(DataReductionProxyRequestOptionsTest, AuthorizationIgnoresEmptyKey) {
  std::string expected_header;
  SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, std::string(),
                        &expected_header);
  CreateRequestOptions(kVersion);
  VerifyExpectedHeader(expected_header, kPageIdValue);

  // Now set an empty key. The auth handler should ignore that, and the key
  // remains |kTestKey|.
  request_options()->SetKeyForTesting(std::string());
  VerifyExpectedHeader(expected_header, kPageIdValue);
}

TEST_F(DataReductionProxyRequestOptionsTest, SecureSession) {
  std::string expected_header;
  SetHeaderExpectations(kSecureSession, kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, std::string(),
                        &expected_header);

  CreateRequestOptions(kVersion);
  request_options()->SetSecureSession(kSecureSession);
  VerifyExpectedHeader(expected_header, kPageIdValue);
}

TEST_F(DataReductionProxyRequestOptionsTest, CallsHeaderCallback) {
  std::string expected_header;
  SetHeaderExpectations(kSecureSession, kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, std::string(),
                        &expected_header);

  CreateRequestOptionsWithCallback(kVersion);
  request_options()->SetSecureSession(kSecureSession);
  VerifyExpectedHeader(expected_header, kPageIdValue);

  std::string callback_header;
  callback_headers().GetHeader(kChromeProxyHeader, &callback_header);
  // |callback_header| does not include a page id. Since the page id is always
  // the last element in the header, check that |callback_header| is the prefix
  // of |expected_header|.
  EXPECT_TRUE(base::StartsWith(expected_header, callback_header,
                               base::CompareCase::SENSITIVE));
}

TEST_F(DataReductionProxyRequestOptionsTest, ParseExperiments) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyExperiment,
      "staging,foo,bar");
  std::string expected_header;
  SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, "staging,foo,bar",
                        &expected_header);

  CreateRequestOptions(kVersion);
  VerifyExpectedHeader(expected_header, kPageIdValue);
}

TEST_F(DataReductionProxyRequestOptionsTest, ParseExperimentsFromFieldTrial) {
  const char kFieldTrialGroupFoo[] = "enabled_foo";
  const char kExperimentFoo[] = "foo";
  const char kExperimentBar[] = "bar";
  const struct {
    std::string field_trial_group;
    std::string command_line_experiment;
    bool disable_server_experiments_via_flag;
    std::string expected_experiment;
  } tests[] = {
      // Disabled field trial groups.
      {"disabled_group", std::string(), false, std::string()},
      {"disabled_group", kExperimentFoo, false, kExperimentFoo},
      // Valid field trial groups should pick from field trial.
      {kFieldTrialGroupFoo, std::string(), false, kExperimentFoo},
      {kFieldTrialGroupFoo, std::string(), true, std::string()},
      // Experiments from command line switch should override.
      {kFieldTrialGroupFoo, kExperimentBar, false, kExperimentBar},
      {kFieldTrialGroupFoo, kExperimentBar, false, kExperimentBar},
      {kFieldTrialGroupFoo, kExperimentBar, true, std::string()},
  };

  std::map<std::string, std::string> server_experiment_foo;

  server_experiment_foo[params::GetDataSaverServerExperimentsOptionName()] =
      kExperimentFoo;

  for (const auto& test : tests) {
    // Remove all related switches first to reset the test state.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        data_reduction_proxy::switches::kDataReductionProxyExperiment);
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDataReductionProxyServerExperimentsDisabled);

    std::string expected_experiments;

    base::test::ScopedFeatureList scoped_feature_list;

    if (test.field_trial_group != "disabled_group") {
      scoped_feature_list.InitWithFeaturesAndParameters(
          {{data_reduction_proxy::features::
                kDataReductionProxyServerExperiments,
            {server_experiment_foo}}},
          {});
    }

    if (test.disable_server_experiments_via_flag) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyServerExperimentsDisabled, "");
    } else {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          data_reduction_proxy::switches::kDataReductionProxyExperiment,
          test.command_line_experiment);
    }

    std::string expected_header;

    if (!test.expected_experiment.empty())
      expected_experiments = test.expected_experiment;

    SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                          kExpectedPatch, kPageId, expected_experiments,
                          &expected_header);

    CreateRequestOptions(kVersion);
    VerifyExpectedHeader(expected_header, kPageIdValue);
  }
}

TEST_F(DataReductionProxyRequestOptionsTest, TestExperimentPrecedence) {
  // Tests that combinations of configurations that trigger "exp=" directive in
  // the Chrome-Proxy header have the right precendence, and only append a value
  // for the highest priority value.

  // Field trial has the lowest priority.
  std::map<std::string, std::string> server_experiment;
  server_experiment[params::GetDataSaverServerExperimentsOptionName()] = "foo";

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeaturesAndParameters(
      {{data_reduction_proxy::features::kDataReductionProxyServerExperiments,
        {server_experiment}}},
      {});

  std::string expected_experiments = "foo";
  std::string expected_header;
  SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, expected_experiments,
                        &expected_header);
  CreateRequestOptions(kVersion);
  VerifyExpectedHeader(expected_header, kPageIdValue);

  // Setting the experiment explicitly has the highest priority.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyExperiment, "bar");
  expected_experiments = "bar";
  SetHeaderExpectations(std::string(), kClientStr, kExpectedBuild,
                        kExpectedPatch, kPageId, expected_experiments,
                        &expected_header);
  CreateRequestOptions(kVersion);
  VerifyExpectedHeader(expected_header, kPageIdValue);
}

TEST_F(DataReductionProxyRequestOptionsTest, GetSessionKeyFromRequestHeaders) {
  const struct {
    std::string chrome_proxy_header_key;
    std::string chrome_proxy_header_value;
    std::string expected_session_key;
    bool expect_result;
  } tests[] = {
      {"chrome-proxy", "something=something_else, s=123, key=value", "123",
       true},
      {"chrome-proxy", "something=something_else, s= 123  456 , key=value",
       "123  456", true},
      {"chrome-proxy", "something=something_else, s=123456,    key=value",
       "123456", true},
      {"chrome-proxy", "something=something else, s=123456,    key=value",
       "123456", true},
      {"chrome-proxy", "something=something else, s=123456  ", "123456", true},
      {"chrome-proxy", "something=something_else, s=, key=value", "", false},
      {"chrome-proxy", "something=something_else, key=value", "", false},
      {"chrome-proxy", "s=123", "123", true},
      {"chrome-proxy", " s = 123 ", "123", true},
      {"some_other_header", "s=123", "", false},
  };

  for (const auto& test : tests) {
    net::HttpRequestHeaders request_headers;
    request_headers.SetHeader("some_random_header_before", "some_random_key");
    request_headers.SetHeader(test.chrome_proxy_header_key,
                              test.chrome_proxy_header_value);
    request_headers.SetHeader("some_random_header_after", "some_random_key");

    base::Optional<std::string> session_key =
        request_options()->GetSessionKeyFromRequestHeaders(request_headers);
    EXPECT_EQ(test.expect_result, session_key.has_value());
    if (test.expect_result) {
      EXPECT_EQ(test.expected_session_key, session_key)
          << test.chrome_proxy_header_key << ":"
          << test.chrome_proxy_header_value;
    }
  }
}

TEST_F(DataReductionProxyRequestOptionsTest, GetPageIdFromRequestHeaders) {
  const struct {
    std::string chrome_proxy_header_key;
    std::string chrome_proxy_header_value;
    uint64_t expected_page_id;
    bool expect_result;
  } tests[] = {
      {"chrome-proxy", "something=something_else, pid=123, key=value", 123,
       true},
      {"chrome-proxy", "something=something_else, pid= 123 , key=value", 123,
       true},
      {"chrome-proxy", "something=something_else, pid=123456,    key=value",
       123456, true},
      {"chrome-proxy", "something=something else, pid=123456,    key=value",
       123456, true},
      {"chrome-proxy", "something=something else, pid=123456  ", 123456, true},
      {"chrome-proxy", "something=something_else, pid=, key=value", 0, false},
      {"chrome-proxy", "something=something_else, key=value", 0, false},
      {"chrome-proxy", "pid=123", 123, true},
      {"chrome-proxy", "pid=123abc", 1194684, true},
      {"chrome-proxy", " pid = 123 ", 123, true},
      {"some_other_header", "pid=123", 0, false},
  };

  for (const auto& test : tests) {
    net::HttpRequestHeaders request_headers;
    request_headers.SetHeader("some_random_header_before", "some_random_key");
    request_headers.SetHeader(test.chrome_proxy_header_key,
                              test.chrome_proxy_header_value);
    request_headers.SetHeader("some_random_header_after", "some_random_key");

    base::Optional<uint64_t> page_id =
        request_options()->GetPageIdFromRequestHeaders(request_headers);
    EXPECT_EQ(test.expect_result, page_id.has_value());
    if (test.expect_result) {
      EXPECT_EQ(test.expected_page_id, page_id)
          << test.chrome_proxy_header_key << ":"
          << test.chrome_proxy_header_value;
    }
  }
}

TEST_F(DataReductionProxyRequestOptionsTest, PageIdIncrementing) {
  CreateRequestOptions(kVersion);
  uint64_t page_id = request_options()->GeneratePageId();
  DCHECK_EQ(++page_id, request_options()->GeneratePageId());
  DCHECK_EQ(++page_id, request_options()->GeneratePageId());
  DCHECK_EQ(++page_id, request_options()->GeneratePageId());

  request_options()->SetSecureSession("blah");

  page_id = request_options()->GeneratePageId();
  DCHECK_EQ(++page_id, request_options()->GeneratePageId());
  DCHECK_EQ(++page_id, request_options()->GeneratePageId());
  DCHECK_EQ(++page_id, request_options()->GeneratePageId());
}

}  // namespace data_reduction_proxy
