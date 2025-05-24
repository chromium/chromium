// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/policy/external_storage/external_storage_policy_controller.h"

#include <optional>

#include "base/values.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// SetAllowlist adds devices 1 and 3 to the allowlist.
const DeviceId kDevice1{0, 0};
const DeviceId kDevice2{1, 2};
const DeviceId kDevice3{34, 56};

}  // namespace

class ExternalStoragePolicyControllerTest : public testing::Test {
 public:
  ExternalStoragePolicyControllerTest() {
    disks::prefs::RegisterProfilePrefs(pref_service_.registry());
  }

  void SetExternalStorageDisabled(bool value) {
    pref_service_.SetBoolean(disks::prefs::kExternalStorageDisabled, value);
  }

  void SetExternalStorageReadOnly(bool value) {
    pref_service_.SetBoolean(disks::prefs::kExternalStorageReadOnly, value);
  }

  void SetAllowlist() {
    auto allowlist =
        base::Value::List().Append(kDevice1.ToDict()).Append(kDevice3.ToDict());
    pref_service_.SetList(disks::prefs::kExternalStorageAllowlist,
                          std::move(allowlist));
  }

  bool IsDeviceAllowlisted(std::optional<DeviceId> device_id) {
    return ExternalStoragePolicyController::IsDeviceAllowlisted(pref_service_,
                                                                device_id);
  }

  bool IsDeviceDisabled(std::optional<DeviceId> device_id) {
    return ExternalStoragePolicyController::IsDeviceDisabled(pref_service_,
                                                             device_id);
  }

  bool IsDeviceReadOnly(std::optional<DeviceId> device_id) {
    return ExternalStoragePolicyController::IsDeviceReadOnly(pref_service_,
                                                             device_id);
  }

  bool IsDeviceWriteable(std::optional<DeviceId> device_id) {
    return ExternalStoragePolicyController::IsDeviceWriteable(pref_service_,
                                                              device_id);
  }

 protected:
  TestingPrefServiceSimple pref_service_;
};

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceAllowlisted_Default) {
  EXPECT_FALSE(IsDeviceAllowlisted(kDevice1));
  EXPECT_FALSE(IsDeviceAllowlisted(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceAllowlisted_AllowlistSet) {
  SetAllowlist();

  EXPECT_TRUE(IsDeviceAllowlisted(kDevice1));
  EXPECT_FALSE(IsDeviceAllowlisted(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceDisabled_Default) {
  EXPECT_FALSE(IsDeviceDisabled(kDevice1));
  EXPECT_FALSE(IsDeviceDisabled(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceDisabled_StorageDisabled) {
  SetExternalStorageDisabled(true);

  EXPECT_TRUE(IsDeviceDisabled(kDevice1));
  EXPECT_TRUE(IsDeviceDisabled(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest,
       IsDeviceDisabled_StorageDisabled_AllowlistSet) {
  SetExternalStorageDisabled(true);
  SetAllowlist();

  EXPECT_FALSE(IsDeviceDisabled(kDevice1));
  EXPECT_TRUE(IsDeviceDisabled(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceReadOnly_Default) {
  EXPECT_FALSE(IsDeviceReadOnly(kDevice1));
  EXPECT_FALSE(IsDeviceReadOnly(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceReadOnly_StorageReadOnly) {
  SetExternalStorageReadOnly(true);

  EXPECT_TRUE(IsDeviceReadOnly(kDevice1));
  EXPECT_TRUE(IsDeviceReadOnly(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest,
       IsDeviceReadOnly_StorageReadOnly_AllowlistSet) {
  SetExternalStorageReadOnly(true);
  SetAllowlist();

  EXPECT_FALSE(IsDeviceReadOnly(kDevice1));
  EXPECT_TRUE(IsDeviceReadOnly(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceWriteable_Default) {
  EXPECT_TRUE(IsDeviceWriteable(kDevice1));
  EXPECT_TRUE(IsDeviceWriteable(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceWriteable_StorageDisabled) {
  SetExternalStorageDisabled(true);

  EXPECT_FALSE(IsDeviceWriteable(kDevice1));
  EXPECT_FALSE(IsDeviceWriteable(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest, IsDeviceWriteable_StorageReadOnly) {
  SetExternalStorageReadOnly(true);

  EXPECT_FALSE(IsDeviceWriteable(kDevice1));
  EXPECT_FALSE(IsDeviceWriteable(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest,
       IsDeviceWriteable_StorageDisabledAndReadOnly) {
  SetExternalStorageDisabled(true);
  SetExternalStorageReadOnly(true);

  EXPECT_FALSE(IsDeviceWriteable(kDevice1));
  EXPECT_FALSE(IsDeviceWriteable(kDevice2));
}

TEST_F(ExternalStoragePolicyControllerTest,
       IsDeviceWriteable_StorageDisabledAndReadOnly_AllowlistSet) {
  SetExternalStorageDisabled(true);
  SetExternalStorageReadOnly(true);
  SetAllowlist();

  EXPECT_TRUE(IsDeviceWriteable(kDevice1));
  EXPECT_FALSE(IsDeviceWriteable(kDevice2));
}

}  // namespace policy
