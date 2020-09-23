// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/cups_proxy/proxy_manager.h"

#include <map>

#include "base/test/task_environment.h"
#include "chrome/services/cups_proxy/fake_cups_proxy_service_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cups_proxy {

namespace {

constexpr int kHttpTooManyRequests = 429;

class MyFakeCupsProxyServiceDelegate : public FakeCupsProxyServiceDelegate {
  bool IsPrinterAccessAllowed() const override { return false; }
};

class ProxyManagerTest : public testing::Test {
 public:
  ProxyManagerTest()
      : manager_(ProxyManager::Create(
            {},
            std::make_unique<MyFakeCupsProxyServiceDelegate>())) {}

  // Proxy a dummy request and add the response code to count_.
  void ProxyRequest() const {
    manager_->ProxyRequest({}, {}, {}, {}, {},
                           base::BindOnce(&ProxyManagerTest::Callback,
                                          weak_factory_.GetWeakPtr()));
  }

  // Return the number of times response code status has been received.
  int NumRequestsByStatusCode(int32_t status) const {
    const auto it = count_.find(status);
    return it == count_.end() ? 0 : it->second;
  }

  // Fast forward the task environment's fake clock.
  void FastForwardBy(base::TimeDelta delta) {
    return task_environment_.FastForwardBy(delta);
  }

 private:
  // Add the response code status to count_.
  void Callback(const std::vector<ipp_converter::HttpHeader>&,
                const std::vector<uint8_t>&,
                int32_t status) {
    count_[status]++;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::map<int32_t, int> count_;

  std::unique_ptr<ProxyManager> manager_;

  base::WeakPtrFactory<ProxyManagerTest> weak_factory_{this};
};

// Test a burst of simultaneous function calls.
TEST_F(ProxyManagerTest, ProxyRequestRateLimitBurst) {
  for (int i = 0; i < ProxyManager::kRateLimit + 3; i++)
    ProxyRequest();
  EXPECT_EQ(NumRequestsByStatusCode(kHttpTooManyRequests), 3);
}

// Test a 0.99s gap between two bursts of function calls.
TEST_F(ProxyManagerTest, ProxyRequestRateLimitShortGap) {
  for (int i = 0; i < ProxyManager::kRateLimit + 1; i++) {
    if (i == ProxyManager::kRateLimit / 2)
      FastForwardBy(base::TimeDelta::FromSecondsD(.99));
    ProxyRequest();
  }
  EXPECT_EQ(NumRequestsByStatusCode(kHttpTooManyRequests), 1);
}

// Test that the rate limit is reset after 1.01s.
TEST_F(ProxyManagerTest, ProxyRequestRateLimitLongGap) {
  for (int i = 0; i < ProxyManager::kRateLimit + 1; i++)
    ProxyRequest();
  EXPECT_EQ(NumRequestsByStatusCode(kHttpTooManyRequests), 1);
  FastForwardBy(base::TimeDelta::FromSecondsD(1.01));
  for (int i = 0; i < ProxyManager::kRateLimit + 1; i++)
    ProxyRequest();
  EXPECT_EQ(NumRequestsByStatusCode(kHttpTooManyRequests), 2);
}

// Test that calls at a constant rate below the rate limit are allowed.
TEST_F(ProxyManagerTest, ProxyRequestRateLimitBelow) {
  for (int i = 0; i < ProxyManager::kRateLimit + 10; i++) {
    FastForwardBy(
        base::TimeDelta::FromSecondsD(1.01 / ProxyManager::kRateLimit));
    ProxyRequest();
  }
  EXPECT_EQ(NumRequestsByStatusCode(kHttpTooManyRequests), 0);
}

// Test that calls at a constant rate above the rate limit are blocked.
TEST_F(ProxyManagerTest, ProxyRequestRateLimitAbove) {
  for (int i = 0; i < ProxyManager::kRateLimit + 10; i++) {
    FastForwardBy(
        base::TimeDelta::FromSecondsD(.99 / ProxyManager::kRateLimit));
    ProxyRequest();
  }
  EXPECT_EQ(NumRequestsByStatusCode(kHttpTooManyRequests), 10);
}

}  // namespace

}  // namespace cups_proxy
