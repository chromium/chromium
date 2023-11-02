// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/tether/host_scan_cache.h"

#include <memory>
#include <unordered_map>

#include "chromeos/ash/components/tether/fake_host_scan_cache.h"
#include "chromeos/ash/components/tether/host_scan_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace tether {

namespace {

class TestObserver final : public HostScanCache::Observer {
 public:
  TestObserver() : empty_cache_count_(0) {}

  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;

  uint32_t empty_cache_count() const { return empty_cache_count_; }

  // HostScanCache::Observer:
  void OnCacheBecameEmpty() override { empty_cache_count_++; }

 private:
  uint32_t empty_cache_count_;
};

}  // namespace

class HostScanCacheTest : public testing::Test {
 public:
  HostScanCacheTest(const HostScanCacheTest&) = delete;
  HostScanCacheTest& operator=(const HostScanCacheTest&) = delete;

 protected:
  HostScanCacheTest()
      : test_entries_(host_scan_test_util::CreateTestEntries()) {}

  void SetUp() override {
    host_scan_cache_ = std::make_unique<FakeHostScanCache>();
    observer_ = std::make_unique<TestObserver>();

    host_scan_cache_->AddObserver(observer_.get());
  }

  void TearDown() override {
    host_scan_cache_->RemoveObserver(observer_.get());
  }

  const std::unordered_map<std::string, HostScanCacheEntry> test_entries_;

  std::unique_ptr<FakeHostScanCache> host_scan_cache_;
  std::unique_ptr<TestObserver> observer_;
};

TEST_F(HostScanCacheTest, TestSetAndRemove) {
  // Set and remove a single entry
  host_scan_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid0));
  EXPECT_FALSE(host_scan_cache_->empty());
  EXPECT_EQ(0u, observer_->empty_cache_count());

  host_scan_cache_->RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  EXPECT_TRUE(host_scan_cache_->empty());
  EXPECT_EQ(1u, observer_->empty_cache_count());

  // Set and remove multiple entries
  host_scan_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid0));
  EXPECT_FALSE(host_scan_cache_->empty());

  host_scan_cache_->SetHostScanResult(
      test_entries_.at(host_scan_test_util::kTetherGuid1));
  EXPECT_FALSE(host_scan_cache_->empty());

  host_scan_cache_->RemoveHostScanResult(host_scan_test_util::kTetherGuid0);
  EXPECT_FALSE(host_scan_cache_->empty());
  EXPECT_EQ(1u, observer_->empty_cache_count());

  host_scan_cache_->RemoveHostScanResult(host_scan_test_util::kTetherGuid1);
  EXPECT_TRUE(host_scan_cache_->empty());
  EXPECT_EQ(2u, observer_->empty_cache_count());
}

}  // namespace tether

}  // namespace ash
