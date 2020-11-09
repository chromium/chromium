// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/generation/password_requirements_spec_fetcher_impl.h"

#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/proto/password_requirements.pb.h"
#include "components/autofill/core/browser/proto/password_requirements_shard.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

// URL prefix for spec requests.
#define SERVER_URL \
  "https://www.gstatic.com/chrome/autofill/password_generation_specs/"

TEST(PasswordRequirementsSpecFetcherTest, FetchData) {
  using ResultCode = PasswordRequirementsSpecFetcherImpl::ResultCode;

  // An empty spec is returned for all error cases (time outs, server responding
  // with anything but HTTP_OK).
  PasswordRequirementsSpec empty_spec;

  PasswordRequirementsSpec success_spec_for_example_com;
  success_spec_for_example_com.set_min_length(17);
  PasswordRequirementsSpec success_spec_for_m_example_com;
  success_spec_for_m_example_com.set_min_length(18);
  PasswordRequirementsSpec spec_for_ip;
  success_spec_for_m_example_com.set_min_length(19);
  PasswordRequirementsSpec success_spec_for_uber_example_com;
  success_spec_for_uber_example_com.set_min_length(20);

  std::string serialized_shard;
  PasswordRequirementsShard shard;
  (*shard.mutable_specs())["example.com"] = success_spec_for_example_com;
  (*shard.mutable_specs())["m.example.com"] = success_spec_for_m_example_com;
  // This spec is stored in the buffer but is not expected to be processed.
  // Only real hostnames are supposed to be parsed.
  (*shard.mutable_specs())["192.168.1.1"] = spec_for_ip;
  // Punycoded entry.
  (*shard.mutable_specs())["xn--ber-example-shb.com"] =
      success_spec_for_uber_example_com;
  shard.SerializeToString(&serialized_shard);

  // If this magic timeout value is set, simulate a timeout.
  const int kMagicTimeout = 10;

  struct {
    // Name of the test for log output.
    const char* test_name;

    // Origin for which the spec is fetched.
    const char* origin;

    // Current configuration that would be set via Finch.
    int generation = 1;
    int prefix_length = 32;
    int timeout = 1000;

    // Handler for the spec requests.
    const char* requested_url;
    std::string response_content;
    net::HttpStatusCode response_status = net::HTTP_OK;

    // Expected spec.
    PasswordRequirementsSpec* expected_spec;
    ResultCode expected_result;
  } tests[] = {
      {
          .test_name = "Business as usual",
          .origin = "https://www.example.com",
          // See echo -n example.com | md5sum | cut -b 1-4
          .requested_url = SERVER_URL "1/5aba",
          .response_content = serialized_shard,
          .expected_spec = &success_spec_for_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "Parts before the eTLD+1 don't matter",
          // m.example.com instead of www.example.com creates the same hash
          // prefix.
          .origin = "https://m.example.com",
          .requested_url = SERVER_URL "1/5aba",
          .response_content = serialized_shard,
          // But shard contains a special entry for m.example.com, so verify
          // that the more specific element is returned.
          .expected_spec = &success_spec_for_m_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "The generation is encoded in the url",
          .origin = "https://www.example.com",
          // Here the test differs from the default:
          .generation = 2,
          .requested_url = SERVER_URL "2/5aba",
          .response_content = serialized_shard,
          .expected_spec = &success_spec_for_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "Shorter prefixes are reflected in the URL",
          .origin = "https://m.example.com",
          // The prefix "5abc" starts with 0b01011010. If the prefix is limited
          // to the first 3 bits, b0100 = 0x4 remains and the rest is zeroed
          // out.
          .prefix_length = 3,
          .requested_url = SERVER_URL "1/4000",
          .response_content = serialized_shard,
          .expected_spec = &success_spec_for_m_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "Simulate a 404 response",
          .origin = "https://www.example.com",
          .requested_url = SERVER_URL "1/5aba",
          .response_content = "Not found",
          // If a file is not found on the server, the spec should be empty.
          .response_status = net::HTTP_NOT_FOUND,
          .expected_spec = &empty_spec,
          .expected_result = ResultCode::kErrorFailedToFetch,
      },
      {
          .test_name = "Simulate a timeout",
          .origin = "https://www.example.com",
          // This simulates a timeout.
          .timeout = kMagicTimeout,
          // This makes sure that the server does not respond by itself as
          // TestURLLoaderFactory reacts as if a response has been added to
          // a URL.
          .requested_url = SERVER_URL "dont_respond",
          .response_content = serialized_shard,
          .expected_spec = &empty_spec,
          .expected_result = ResultCode::kErrorTimeout,
      },
      {
          .test_name = "Zero prefix",
          .origin = "https://www.example.com",
          // A zero prefix will be the hard-coded Finch default and should work.
          .prefix_length = 0,
          .requested_url = SERVER_URL "1/0000",
          .response_content = serialized_shard,
          .expected_spec = &success_spec_for_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "IP addresses give the empty spec",
          .origin = "http://192.168.1.1/",
          // By setting the prefix to 0, the URL of the shard is predefined,
          // but actually, not network request should be sent as password
          // requirements are not supported for IP addresses.
          .prefix_length = 0,
          .requested_url = SERVER_URL "0/0000",
          .response_content = serialized_shard,
          .expected_spec = &empty_spec,
          .expected_result = ResultCode::kErrorInvalidOrigin,
      },
      {
          .test_name = "IP addresses give the empty spec",
          .origin = "chrome://settings",
          // By setting the prefix to 0, the URL of the shard is predefined,
          // but actually, not network request should be sent as password
          // requirements are not supported the chrome:// scheme.
          .prefix_length = 0,
          .requested_url = SERVER_URL "0/0000",
          .response_content = serialized_shard,
          .expected_spec = &empty_spec,
          .expected_result = ResultCode::kErrorInvalidOrigin,
      },
      {
          .test_name = "Trailing period is normalized",
          // Despite the trailing '.', everything is like for example.com
          .origin = "https://www.example.com.",
          .requested_url = SERVER_URL "1/5aba",
          .response_content = serialized_shard,
          .expected_spec = &success_spec_for_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "Test punycoding",
          .origin = "https://www.über-example.com",
          // See echo -n xn--ber-example-shb.com | md5sum | cut -b 1-4
          .requested_url = SERVER_URL "1/e5db",
          .response_content = serialized_shard,
          .expected_spec = &success_spec_for_uber_example_com,
          .expected_result = ResultCode::kFoundSpec,
      },
      {
          .test_name = "Test no entry",
          .origin = "https://www.no-entry.com",
          // See echo -n no-entry.com | md5sum | cut -b 1-4
          .requested_url = SERVER_URL "1/7579",
          .response_content = serialized_shard,
          .expected_spec = &empty_spec,
          .expected_result = ResultCode::kFoundNoSpec,
      },
  };

  for (const auto& test : tests) {
    SCOPED_TRACE(test.test_name);
    base::HistogramTester histogram_tester;

    base::test::TaskEnvironment environment(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    network::TestURLLoaderFactory loader_factory;
    loader_factory.AddResponse(test.requested_url, test.response_content,
                               test.response_status);

    // Data to be collected from the callback.
    bool callback_called = false;
    PasswordRequirementsSpec returned_spec;

    // Trigger the network request and record data of the callback.
    PasswordRequirementsSpecFetcherImpl fetcher(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &loader_factory),
        test.generation, test.prefix_length, test.timeout);
    auto callback =
        base::BindLambdaForTesting([&](const PasswordRequirementsSpec& spec) {
          callback_called = true;
          returned_spec = spec;
        });
    fetcher.Fetch(GURL(test.origin), callback);

    environment.RunUntilIdle();

    if (test.timeout == kMagicTimeout) {
      // Make sure that the request takes longer than the timeout and gets
      // killed by the timer.
      environment.FastForwardBy(
          base::TimeDelta::FromMilliseconds(2 * kMagicTimeout));
      environment.RunUntilIdle();
    }

    ASSERT_TRUE(callback_called);
    EXPECT_EQ(test.expected_spec->SerializeAsString(),
              returned_spec.SerializeAsString());
    histogram_tester.ExpectUniqueSample(
        "PasswordManager.RequirementsSpecFetcher.Result", test.expected_result,
        1u);
  }
}

// Test two requests to fetch the same shard before the network responded.
// In this case, only one network request should be sent.
TEST(PasswordRequirementsSpecFetcherTest, FetchDataInterleaved) {
  for (bool simulate_timeout : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "Simulate timeout? " << simulate_timeout);

    // Set up the data that will be served.
    std::string serialized_shard;
    PasswordRequirementsShard shard;
    (*shard.mutable_specs())["a.com"].set_min_length(17);
    (*shard.mutable_specs())["b.com"].set_min_length(18);
    shard.SerializeToString(&serialized_shard);

    base::test::TaskEnvironment environment(
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
    network::TestURLLoaderFactory loader_factory;

    // Target into which data will be written by the callback.
    PasswordRequirementsSpec spec_for_a;
    PasswordRequirementsSpec spec_for_b;
    // Set some values to see whether they get overridden by the callback.
    spec_for_a.set_min_length(1);
    spec_for_b.set_min_length(1);

    const int kVersion = 1;
    // With a prefix length of 0, we guarantee that only one shard exists
    // and therefore that the requests go to the same endpoint and can be
    // unified into one network request.
    const size_t kPrefixLength = 0;
    const int kTimeout = 1000;
    PasswordRequirementsSpecFetcherImpl fetcher(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &loader_factory),
        kVersion, kPrefixLength, kTimeout);

    fetcher.Fetch(
        GURL("http://a.com"),
        base::BindLambdaForTesting(
            [&](const PasswordRequirementsSpec& spec) { spec_for_a = spec; }));
    fetcher.Fetch(
        GURL("http://b.com"),
        base::BindLambdaForTesting(
            [&](const PasswordRequirementsSpec& spec) { spec_for_b = spec; }));

    EXPECT_EQ(1, loader_factory.NumPending());

    if (simulate_timeout) {
      environment.FastForwardBy(
          base::TimeDelta::FromMilliseconds(2 * kTimeout));
      environment.RunUntilIdle();
      EXPECT_FALSE(spec_for_a.has_min_length());
      EXPECT_FALSE(spec_for_b.has_min_length());
    } else {
      loader_factory.AddResponse(SERVER_URL "1/0000", serialized_shard,
                                 net::HTTP_OK);
      environment.RunUntilIdle();

      EXPECT_EQ(17u, spec_for_a.min_length());
      EXPECT_EQ(18u, spec_for_b.min_length());
    }
  }
}

// In case of incognito mode, we won't have a URL loader factory.
// Test that an empty spec is returned by the spec fetcher in this case.
TEST(PasswordRequirementsSpecFetcherTest, FetchDataWithoutURLLoaderFactory) {
  base::test::TaskEnvironment environment;

  // Target into which data will be written by the callback.
  PasswordRequirementsSpec received_spec;
  // Set some values to see whether they get overridden by the callback.
  received_spec.set_min_length(1);

  const int kVersion = 1;
  const size_t kPrefixLength = 0;
  const int kTimeout = 1000;
  PasswordRequirementsSpecFetcherImpl fetcher(nullptr, kVersion, kPrefixLength,
                                              kTimeout);

  fetcher.Fetch(
      GURL("http://a.com"),
      base::BindLambdaForTesting(
          [&](const PasswordRequirementsSpec& spec) { received_spec = spec; }));
  environment.RunUntilIdle();
  EXPECT_FALSE(received_spec.has_min_length());
}

#undef SERVER_URL

}  // namespace

}  // namespace autofill
