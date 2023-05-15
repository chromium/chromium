// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <limits>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::win::RegKey;

class GCAPILastRunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Override keys - this is undone during destruction.
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));

    // Create the client state key in the right places.
    std::wstring reg_path(google_update::kRegPathClientState);
    reg_path += L"\\";
    reg_path += google_update::kChromeUpgradeCode;
    RegKey client_state(HKEY_CURRENT_USER, reg_path.c_str(),
                        KEY_CREATE_SUB_KEY | KEY_WOW64_32KEY);
    ASSERT_TRUE(client_state.Valid());

    // Place a bogus "pv" value in the right places to make the last run
    // checker believe Chrome is installed.
    std::wstring clients_path(google_update::kRegPathClients);
    clients_path += L"\\";
    clients_path += google_update::kChromeUpgradeCode;
    RegKey client_key(HKEY_CURRENT_USER, clients_path.c_str(),
                      KEY_CREATE_SUB_KEY | KEY_SET_VALUE | KEY_WOW64_32KEY);
    ASSERT_TRUE(client_key.Valid());
    client_key.WriteValue(L"pv", L"1.2.3.4");
  }

  bool SetLastRunTime(int64_t last_run_time) {
    return SetLastRunTimeString(base::NumberToWString(last_run_time));
  }

  bool SetLastRunTimeString(const std::wstring& last_run_time_string) {
    const wchar_t* base_path = google_update::kRegPathClientState;
    std::wstring path(base_path);
    path += L"\\";
    path += google_update::kChromeUpgradeCode;

    RegKey client_state(HKEY_CURRENT_USER, path.c_str(),
                        KEY_SET_VALUE | KEY_WOW64_32KEY);
    return (client_state.Valid() &&
            client_state.WriteValue(google_update::kRegLastRunTimeField,
                                    last_run_time_string.c_str()) ==
                ERROR_SUCCESS);
  }

 private:
  registry_util::RegistryOverrideManager override_manager_;
};

TEST_F(GCAPILastRunTest, Basic) {
  Time last_run = Time::NowFromSystemTime() - base::Days(10);
  EXPECT_TRUE(SetLastRunTime(last_run.ToInternalValue()));

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(10, days_since_last_run);
}

TEST_F(GCAPILastRunTest, NoLastRun) {
  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(-1, days_since_last_run);
}

TEST_F(GCAPILastRunTest, InvalidLastRun) {
  EXPECT_TRUE(SetLastRunTimeString(L"Hi Mum!"));
  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(-1, days_since_last_run);
}

TEST_F(GCAPILastRunTest, OutOfRangeLastRun) {
  Time last_run = Time::NowFromSystemTime() - base::Days(-42);
  EXPECT_TRUE(SetLastRunTime(last_run.ToInternalValue()));

  int days_since_last_run = GoogleChromeDaysSinceLastRun();
  EXPECT_EQ(-1, days_since_last_run);
}
