// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/cmtg_key_fetcher.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "components/webauthn/core/browser/cmtg_device_key_provider.h"
#include "components/webauthn/core/browser/fake_cmtg_device_key_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class CmtgKeyFetcherTest : public ::testing::Test {
 protected:
  CmtgKeyFetcherTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    fake_cmtg_device_key_provider_ =
        std::make_unique<webauthn::FakeCmtgDeviceKeyProvider>();
    fetcher_ =
        std::make_unique<CmtgKeyFetcher>(fake_cmtg_device_key_provider_.get(),
                                         task_environment_.GetMockTickClock());
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<webauthn::FakeCmtgDeviceKeyProvider>
      fake_cmtg_device_key_provider_;
  std::unique_ptr<CmtgKeyFetcher> fetcher_;
};

TEST_F(CmtgKeyFetcherTest, Success) {
  std::vector<std::vector<uint8_t>> expected_keys = {{1, 2, 3}};
  fake_cmtg_device_key_provider_->SetNextKeys(expected_keys);

  fetcher_->Start();
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(fetcher_->is_ready());

  // Calling WaitForKeys after the fetcher is ready should immediately invoke
  // the callback.
  base::test::TestFuture<void> future;
  fetcher_->WaitForKeys(future.GetCallback());
  EXPECT_TRUE(future.IsReady());
  EXPECT_EQ(fetcher_->keys(), expected_keys);
}

TEST_F(CmtgKeyFetcherTest, Failure) {
  fake_cmtg_device_key_provider_->SetNextError(
      webauthn::CmtgDeviceKeyProvider::Error::kNetworkError);

  fetcher_->Start();
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_TRUE(fetcher_->is_ready());
  EXPECT_EQ(fetcher_->keys(), std::nullopt);
}

TEST_F(CmtgKeyFetcherTest, Timeout) {
  fake_cmtg_device_key_provider_->SetHoldCallback(true);

  fetcher_->Start();
  base::test::TestFuture<void> future;
  fetcher_->WaitForKeys(future.GetCallback());

  task_environment_.FastForwardBy(
      GPMEnclaveController::kFetchDeviceKeysTimeout - base::Seconds(1));
  EXPECT_FALSE(future.IsReady());

  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(fetcher_->is_ready());
  EXPECT_EQ(fetcher_->keys(), std::nullopt);
}

TEST_F(CmtgKeyFetcherTest, WaitAndResolve) {
  fake_cmtg_device_key_provider_->SetHoldCallback(true);

  fetcher_->Start();
  EXPECT_FALSE(fetcher_->is_ready());
  base::test::TestFuture<void> future;
  fetcher_->WaitForKeys(future.GetCallback());

  const base::TimeDelta wait_time =
      GPMEnclaveController::kFetchDeviceKeysTimeout / 2;
  task_environment_.FastForwardBy(wait_time);
  EXPECT_FALSE(future.IsReady());
  EXPECT_TRUE(fake_cmtg_device_key_provider_->has_pending_callback());

  std::vector<std::vector<uint8_t>> expected_keys = {{4, 5, 6}};
  fake_cmtg_device_key_provider_->ResolvePending(expected_keys);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(fetcher_->is_ready());
  EXPECT_EQ(fetcher_->keys(), expected_keys);

  histogram_tester_.ExpectUniqueTimeSample(
      "WebAuthentication.Cmtg.BlockedDelay", wait_time, 1);
}

TEST_F(CmtgKeyFetcherTest, WaitAndReject) {
  fake_cmtg_device_key_provider_->SetHoldCallback(true);

  fetcher_->Start();
  base::test::TestFuture<void> future;
  fetcher_->WaitForKeys(future.GetCallback());

  const base::TimeDelta wait_time =
      GPMEnclaveController::kFetchDeviceKeysTimeout / 2;
  task_environment_.FastForwardBy(wait_time);
  EXPECT_FALSE(future.IsReady());

  fake_cmtg_device_key_provider_->RejectPending(
      webauthn::CmtgDeviceKeyProvider::Error::kNetworkError);
  EXPECT_TRUE(future.IsReady());
  EXPECT_TRUE(fetcher_->is_ready());
  EXPECT_EQ(fetcher_->keys(), std::nullopt);

  histogram_tester_.ExpectUniqueTimeSample(
      "WebAuthentication.Cmtg.BlockedDelay", wait_time, 1);
}

}  // namespace
