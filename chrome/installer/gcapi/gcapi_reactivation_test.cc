// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/strings/string_number_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/time/time.h"
#include "base/win/registry.h"
#include "chrome/installer/gcapi/gcapi.h"
#include "chrome/installer/gcapi/gcapi_reactivation.h"
#include "chrome/installer/util/google_update_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::win::RegKey;

class GCAPIReactivationTest : public ::testing::Test {
 protected:
  GCAPIReactivationTest() {}

  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  bool SetChromeInstallMarker(HKEY hive) {
    // Create the client state keys in the right places.
    std::wstring reg_path(google_update::kRegPathClients);
    reg_path += L"\\";
    reg_path += google_update::kChromeUpgradeCode;
    RegKey client_state(hive, reg_path.c_str(),
                        KEY_CREATE_SUB_KEY | KEY_SET_VALUE | KEY_WOW64_32KEY);
    return (client_state.Valid() &&
            client_state.WriteValue(google_update::kRegVersionField,
                                    L"1.2.3.4") == ERROR_SUCCESS);
  }

  bool SetLastRunTime(HKEY hive, int64_t last_run_time) {
    return SetLastRunTimeString(hive, base::NumberToWString(last_run_time));
  }

  bool SetLastRunTimeString(HKEY hive,
                            const std::wstring& last_run_time_string) {
    const wchar_t* base_path = (hive == HKEY_LOCAL_MACHINE)
                                   ? google_update::kRegPathClientStateMedium
                                   : google_update::kRegPathClientState;
    std::wstring path(base_path);
    path += L"\\";
    path += google_update::kChromeUpgradeCode;

    RegKey client_state(hive, path.c_str(), KEY_SET_VALUE | KEY_WOW64_32KEY);
    return (client_state.Valid() &&
            client_state.WriteValue(google_update::kRegLastRunTimeField,
                                    last_run_time_string.c_str()) ==
                ERROR_SUCCESS);
  }

  std::wstring GetReactivationString(HKEY hive) {
    const wchar_t* base_path = (hive == HKEY_LOCAL_MACHINE)
                                   ? google_update::kRegPathClientStateMedium
                                   : google_update::kRegPathClientState;
    std::wstring path(base_path);
    path += L"\\";
    path += google_update::kChromeUpgradeCode;

    RegKey client_state(hive, path.c_str(), KEY_QUERY_VALUE | KEY_WOW64_32KEY);
    if (client_state.Valid()) {
      std::wstring actual_brand;
      if (client_state.ReadValue(google_update::kRegRLZReactivationBrandField,
                                 &actual_brand) == ERROR_SUCCESS) {
        return actual_brand;
      }
    }

    return L"ERROR";
  }

  registry_util::RegistryOverrideManager override_manager_;
};

TEST_F(GCAPIReactivationTest, CheckSetReactivationBrandCode) {
  EXPECT_TRUE(SetReactivationBrandCode(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  EXPECT_TRUE(HasBeenReactivated());
}

TEST_F(GCAPIReactivationTest, CanOfferReactivation_Basic) {
  DWORD error;

  // We're not installed yet. Make sure CanOfferReactivation fails.
  EXPECT_FALSE(
      CanOfferReactivation(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(static_cast<DWORD>(REACTIVATE_ERROR_NOTINSTALLED), error);

  // Now pretend to be installed. CanOfferReactivation should pass.
  EXPECT_TRUE(SetChromeInstallMarker(HKEY_CURRENT_USER));
  EXPECT_TRUE(
      CanOfferReactivation(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));

  // Now set a recent last_run value. CanOfferReactivation should fail again.
  Time hkcu_last_run = Time::NowFromSystemTime() - base::Days(20);
  EXPECT_TRUE(
      SetLastRunTime(HKEY_CURRENT_USER, hkcu_last_run.ToInternalValue()));
  EXPECT_FALSE(
      CanOfferReactivation(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(static_cast<DWORD>(REACTIVATE_ERROR_NOTDORMANT), error);

  // Now set a last_run value that exceeds the threshold.
  hkcu_last_run =
      Time::NowFromSystemTime() - base::Days(kReactivationMinDaysDormant);
  EXPECT_TRUE(
      SetLastRunTime(HKEY_CURRENT_USER, hkcu_last_run.ToInternalValue()));
  EXPECT_TRUE(
      CanOfferReactivation(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));

  // Test some invalid inputs
  EXPECT_FALSE(
      CanOfferReactivation(nullptr, GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(static_cast<DWORD>(REACTIVATE_ERROR_INVALID_INPUT), error);

  // One more valid one
  EXPECT_TRUE(
      CanOfferReactivation(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));

  // Check that the previous brands check works:
  EXPECT_TRUE(
      SetReactivationBrandCode(L"GOOGOO", GCAPI_INVOKED_STANDARD_SHELL));
  EXPECT_FALSE(
      CanOfferReactivation(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(static_cast<DWORD>(REACTIVATE_ERROR_ALREADY_REACTIVATED), error);
}

TEST_F(GCAPIReactivationTest, Reactivation_Flow) {
  DWORD error;

  // Set us up as a candidate for reactivation.
  EXPECT_TRUE(SetChromeInstallMarker(HKEY_CURRENT_USER));

  Time hkcu_last_run =
      Time::NowFromSystemTime() - base::Days(kReactivationMinDaysDormant);
  EXPECT_TRUE(
      SetLastRunTime(HKEY_CURRENT_USER, hkcu_last_run.ToInternalValue()));

  EXPECT_TRUE(ReactivateChrome(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  // Make sure we can't reactivate again:
  EXPECT_FALSE(ReactivateChrome(L"GAGA", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(static_cast<DWORD>(REACTIVATE_ERROR_ALREADY_REACTIVATED), error);

  // Should not be able to reactivate under other brands:
  EXPECT_FALSE(ReactivateChrome(L"MAMA", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));

  // Validate that previous_brands are rejected:
  EXPECT_FALSE(ReactivateChrome(L"PFFT", GCAPI_INVOKED_STANDARD_SHELL, &error));
  EXPECT_EQ(static_cast<DWORD>(REACTIVATE_ERROR_ALREADY_REACTIVATED), error);
  EXPECT_EQ(L"GAGA", GetReactivationString(HKEY_CURRENT_USER));
}
