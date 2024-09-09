// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MANTA_BASE_PROVIDER_TEST_HELPER_H_
#define COMPONENTS_MANTA_BASE_PROVIDER_TEST_HELPER_H_

#include "base/test/task_environment.h"
#include "components/manta/base_provider.h"
#include "components/manta/manta_service_callbacks.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace manta {

// Helper utilities that take care of common fakes, useful for unit test of
// particular providers.
// A provider (e.g. FooProvider) unittest should have:
//   * a FakeFooProvider that extends both FooProvider and FakeBaseProvider so
//     that it mocks the FooProvider's functions by using
//     `FakeBaseProvider::RequestInternal`.
//   * a FooProviderTest fixture that extends BaseProviderTest and implement a
//     `CreateFooProvider` function to return a FakeFooProvider instance.

class FakeBaseProvider : virtual public BaseProvider {
 public:
  FakeBaseProvider(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);

  FakeBaseProvider(const FakeBaseProvider&) = delete;
  FakeBaseProvider& operator=(const FakeBaseProvider&) = delete;

  ~FakeBaseProvider() override;

 protected:
  // Overrides BaseProvider:
  void RequestInternal(const GURL& url,
                       const std::string& oauth_consumer_name,
                       const net::NetworkTrafficAnnotationTag& annotation_tag,
                       manta::proto::Request& request,
                       const MantaMetricType metric_type,
                       MantaProtoResponseCallback done_callback,
                       const base::TimeDelta timeout) override;
};

class BaseProviderTest : public testing::Test {
 public:
  BaseProviderTest();

  BaseProviderTest(const BaseProviderTest&) = delete;
  BaseProviderTest& operator=(const BaseProviderTest&) = delete;

  ~BaseProviderTest() override;

  // Overrides testing::Test:
  void SetUp() override;

  void SetEndpointMockResponse(const GURL& request_url,
                               const std::string& response_data,
                               net::HttpStatusCode response_code,
                               net::Error error);

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

}  // namespace manta

#endif  // COMPONENTS_MANTA_BASE_PROVIDER_TEST_HELPER_H_
