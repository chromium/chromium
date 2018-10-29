// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/components/system_restore_point_component.h"

#include "base/synchronization/lock.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/chrome_cleaner/test/test_branding.h"
#include "testing/gtest/include/gtest/gtest.h"

// TODO(robertshield): Figure out how to test this. Near as I can tell the
// only way to enumerate RestorePoints is via WMI, which is 12 kinds of ugh.

namespace chrome_cleaner {

namespace {

constexpr DWORD kFakeSeqNumber = 42U;
bool g_set_called = false;
bool g_remove_called = false;

base::Lock* SharedLock() {
  static base::Lock lock;
  return &lock;
}

BOOL WINAPI FakeSetRestorePointInfoWFn(RESTOREPOINTINFOW* restore_point_info,
                                       STATEMGRSTATUS* state_mgr_status) {
  state_mgr_status->llSequenceNumber = kFakeSeqNumber;
  state_mgr_status->nStatus = 0;
  g_set_called = true;
  return true;
}

BOOL WINAPI FakeRemoveRestorePointFn(DWORD sequence_number) {
  EXPECT_EQ(kFakeSeqNumber, sequence_number);
  g_remove_called = true;
  return true;
}

BOOL WINAPI
FakeSetRestorePointInfoWFailFn(RESTOREPOINTINFOW* restore_point_info,
                               STATEMGRSTATUS* state_mgr_status) {
  state_mgr_status->llSequenceNumber = kFakeSeqNumber;
  state_mgr_status->nStatus = 0;
  g_set_called = true;
  return false;
}

BOOL WINAPI FakeRemoveRestorePointFailFn(DWORD sequence_number) {
  EXPECT_EQ(kFakeSeqNumber, sequence_number);
  g_remove_called = true;
  return false;
}

}  // namespace

class TestSystemRestorePointComponent : public SystemRestorePointComponent {
 public:
  TestSystemRestorePointComponent()
      : SystemRestorePointComponent(TEST_PRODUCT_FULLNAME_STRING) {}

  void OverrideSetRestorePointInfoWFn(
      SystemRestorePointComponent::SetRestorePointInfoWFn func) {
    set_restore_point_info_fn_ = func;
  }
  void OverrideRemoveRestorePointFn(
      SystemRestorePointComponent::RemoveRestorePointFn func) {
    remove_restore_point_info_fn_ = func;
  }
};

class SystemRestorePointComponentTest : public testing::Test {
 protected:
  SystemRestorePointComponentTest() : auto_lock_(*SharedLock()) {}

  void SetUp() override {
    system_restore_point_component_.OverrideSetRestorePointInfoWFn(
        &FakeSetRestorePointInfoWFn);
    system_restore_point_component_.OverrideRemoveRestorePointFn(
        &FakeRemoveRestorePointFn);

    registry_override_.OverrideRegistry(HKEY_LOCAL_MACHINE);
    g_set_called = false;
    g_remove_called = false;
  }

  base::AutoLock auto_lock_;

  TestSystemRestorePointComponent system_restore_point_component_;
  registry_util::RegistryOverrideManager registry_override_;
};

TEST_F(SystemRestorePointComponentTest, CheckRestoreCallsSuccess) {
  system_restore_point_component_.PreCleanup();
  system_restore_point_component_.PostCleanup(RESULT_CODE_SUCCESS, nullptr);
  EXPECT_TRUE(g_set_called);
  EXPECT_FALSE(g_remove_called);
}

TEST_F(SystemRestorePointComponentTest, CheckRestoreCallsFailure) {
  system_restore_point_component_.PreCleanup();
  system_restore_point_component_.PostCleanup(RESULT_CODE_FAILED, nullptr);
  EXPECT_TRUE(g_set_called);
  EXPECT_TRUE(g_remove_called);
}

TEST_F(SystemRestorePointComponentTest, CheckRestoreCallsRestorePointFailure) {
  system_restore_point_component_.OverrideSetRestorePointInfoWFn(
      &FakeSetRestorePointInfoWFailFn);
  system_restore_point_component_.PreCleanup();
  system_restore_point_component_.PostCleanup(RESULT_CODE_SUCCESS, nullptr);
  EXPECT_TRUE(g_set_called);
  EXPECT_FALSE(g_remove_called);
}

TEST_F(SystemRestorePointComponentTest, CheckRestoreCommitRestorePointFailure) {
  system_restore_point_component_.OverrideRemoveRestorePointFn(
      &FakeRemoveRestorePointFailFn);
  system_restore_point_component_.PreCleanup();
  system_restore_point_component_.PostCleanup(RESULT_CODE_FAILED, nullptr);
  EXPECT_TRUE(g_set_called);
  EXPECT_TRUE(g_remove_called);
}

}  // namespace chrome_cleaner
