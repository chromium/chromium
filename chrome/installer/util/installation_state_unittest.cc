// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/installation_state.h"

#include "base/strings/cstring_view.h"
#include "base/strings/strcat.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace installer {

class FindProductGuidTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        hklm_override_.OverrideRegistry(HKEY_LOCAL_MACHINE));

    // Create Uninstall entries for two products.
    base::win::RegKey uninstall(HKEY_LOCAL_MACHINE, kUninstallRootKey.c_str(),
                                KEY_CREATE_SUB_KEY);
    ASSERT_TRUE(uninstall.Valid());
    base::win::RegKey product1(
        uninstall.Handle(), base::StrCat({L"{", kProductGuid1, L"}"}).c_str(),
        KEY_SET_VALUE);
    ASSERT_TRUE(product1.Valid());
    ASSERT_EQ(product1.WriteValue(kUninstallDisplayNameField, L"FOOFOO"),
              ERROR_SUCCESS);
    base::win::RegKey product2(
        uninstall.Handle(), base::StrCat({L"{", kProductGuid2, L"}"}).c_str(),
        KEY_SET_VALUE);
    ASSERT_TRUE(product2.Valid());
    ASSERT_EQ(product2.WriteValue(kUninstallDisplayNameField, L"BARBAR"),
              ERROR_SUCCESS);
  }

  static constexpr base::wcstring_view kUninstallRootKey{
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall"};
  // A product named "FOOFOO".
  static constexpr base::wcstring_view kProductGuid1{
      L"410977A7-158D-327D-AEBC-66E5239FC916"};
  // A product named "BARBAR".
  static constexpr base::wcstring_view kProductGuid2{
      L"510977A7-158D-327D-AEBC-66E5239FC916"};

 private:
  registry_util::RegistryOverrideManager hklm_override_;
};

// Tests that FindProductGuid works when no hint is provided.
TEST_F(FindProductGuidTest, WithoutHint) {
  EXPECT_EQ(ProductState::FindProductGuid(L"FOOFOO", {}),
            kProductGuid1.c_str());
  EXPECT_EQ(ProductState::FindProductGuid(L"BARBAR", {}),
            kProductGuid2.c_str());
}

// Tests that FindProductGuid works when a hint is provided to the desired item.
TEST_F(FindProductGuidTest, WithCorrectHint) {
  EXPECT_EQ(ProductState::FindProductGuid(L"FOOFOO", kProductGuid1),
            kProductGuid1.c_str());
  EXPECT_EQ(ProductState::FindProductGuid(L"BARBAR", kProductGuid2),
            kProductGuid2.c_str());
}

// Tests that FindProductGuid works when a hint is provided to the wrong item.
TEST_F(FindProductGuidTest, WithIncorrectHint) {
  EXPECT_EQ(ProductState::FindProductGuid(L"FOOFOO", kProductGuid2),
            kProductGuid1.c_str());
  EXPECT_EQ(ProductState::FindProductGuid(L"BARBAR", kProductGuid1),
            kProductGuid2.c_str());
}

}  // namespace installer
