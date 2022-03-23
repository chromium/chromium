// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_cellular_pref_handler.h"

#include "base/test/task_environment.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

const char kIccid[] = "1234567890";
const char kSmdpAddress[] = "LPA:1$SmdpAddress$ActivationCode";

}  // namespace

class ManagedCellularPrefHandlerTest : public testing::Test {
 protected:
  ManagedCellularPrefHandlerTest() = default;
  ~ManagedCellularPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    ManagedCellularPrefHandler::RegisterLocalStatePrefs(
        device_prefs_.registry());
  }

  void TearDown() override { managed_cellular_pref_handler_.reset(); }

  void Init() {
    managed_cellular_pref_handler_ =
        std::make_unique<ManagedCellularPrefHandler>();
  }

  void SetDevicePrefs(bool set_to_null = false) {
    managed_cellular_pref_handler_->SetDevicePrefs(
        set_to_null ? nullptr : &device_prefs_);
  }

  void AddIccidSmdpPair(const std::string& iccid,
                        const std::string& smdp_address) {
    managed_cellular_pref_handler_->AddIccidSmdpPair(iccid, smdp_address);
  }

  void RemovePairForIccid(const std::string& iccid) {
    managed_cellular_pref_handler_->RemovePairWithIccid(iccid);
  }

  const std::string* GetSmdpAddressFromIccid(const std::string& iccid) {
    return managed_cellular_pref_handler_->GetSmdpAddressFromIccid(iccid);
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple device_prefs_;

  std::unique_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_;
};

TEST_F(ManagedCellularPrefHandlerTest, AddRemoveIccidSmdpPair) {
  Init();
  SetDevicePrefs();

  // Add a pair of ICCID - SMDP address pair to pref and verify that the correct
  // value can be retrieved.
  AddIccidSmdpPair(kIccid, kSmdpAddress);
  const std::string* smdp_address = GetSmdpAddressFromIccid(kIccid);
  EXPECT_TRUE(smdp_address);
  EXPECT_EQ(kSmdpAddress, *smdp_address);
  EXPECT_FALSE(GetSmdpAddressFromIccid("00000000000"));
  RemovePairForIccid(kIccid);
  smdp_address = GetSmdpAddressFromIccid(kIccid);
  EXPECT_FALSE(smdp_address);
}

TEST_F(ManagedCellularPrefHandlerTest, NoDevicePrefSet) {
  Init();
  SetDevicePrefs(/*set_to_null=*/true);

  // Verify that when there's no device prefs, no SMDP address can be
  // retrieved.
  const std::string* smdp_address = GetSmdpAddressFromIccid(kIccid);
  EXPECT_FALSE(smdp_address);
  AddIccidSmdpPair(kIccid, kSmdpAddress);
  smdp_address = GetSmdpAddressFromIccid(kIccid);
  EXPECT_FALSE(smdp_address);
}

}  // namespace chromeos
