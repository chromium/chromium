// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/nearby/presence/nearby_presence_service_impl.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace ash::nearby::presence {

namespace {

class FakeScanDelegate : public NearbyPresenceService::ScanDelegate {
 public:
  FakeScanDelegate() = default;
  FakeScanDelegate(const FakeScanDelegate&) = delete;
  FakeScanDelegate& operator=(const FakeScanDelegate&) = delete;
  ~FakeScanDelegate() override = default;

  void OnPresenceDeviceFound(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    found_called = true;
  }
  void OnPresenceDeviceChanged(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    changed_called = true;
  }
  void OnPresenceDeviceLost(
      const NearbyPresenceService::PresenceDevice& presence_device) override {
    lost_called = true;
  }

  bool WasOnPresenceDeviceFoundCalled() { return found_called; }

 private:
  bool found_called = false;
  bool changed_called = false;
  bool lost_called = false;
};

}  // namespace

class NearbyPresenceServiceImplTest : public testing::Test {
 public:
  NearbyPresenceServiceImplTest() = default;
  ~NearbyPresenceServiceImplTest() override = default;

  // testing::Test:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    nearby_presence_service =
        std::make_unique<NearbyPresenceServiceImpl>(pref_service_.get());
  }

  std::unique_ptr<NearbyPresenceServiceImpl> nearby_presence_service;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(NearbyPresenceServiceImplTest, StartScan) {
  NearbyPresenceService::ScanFilter filter;
  FakeScanDelegate scan_delegate;

  // Call start scan and verify it calls the OnPresenceDeviceFound delegate
  // function.
  nearby_presence_service->StartScan(filter, &scan_delegate);
  EXPECT_TRUE(scan_delegate.WasOnPresenceDeviceFoundCalled());
}

}  // namespace ash::nearby::presence
