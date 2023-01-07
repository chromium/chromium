// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_TEST_BASE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_TEST_BASE_H_

#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

// Base class for testing prefetch requests with simulated responses.
class PrefetchRequestTestBase : public testing::Test {
 public:
  static const char kExperimentValueSetInFieldTrial[];

  PrefetchRequestTestBase();
  ~PrefetchRequestTestBase() override;

  void SetUp() override;

  void SetUpExperimentOption();

  void RespondWithNetError(int net_error);
  void RespondWithHttpError(net::HttpStatusCode http_error);
  void RespondWithData(const std::string& data);
  void RespondWithHttpErrorAndData(net::HttpStatusCode http_error,
                                   const std::string& data);
  network::TestURLLoaderFactory::PendingRequest* GetPendingRequest(
      size_t index = 0);

  std::string GetExperiementHeaderValue(
      network::TestURLLoaderFactory::PendingRequest* pending_request);

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

  void RunUntilIdle();
  void FastForwardBy(base::TimeDelta delta);
  void FastForwardUntilNoTasksRemain();

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::ResourceRequest last_resource_request_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_REQUEST_TEST_BASE_H_
