// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/omnibox_everywhere/omnibox_everywhere_prefs.h"

#include <memory>
#include <utility>

#include "chrome/browser/new_tab_page/prefs/ntp_pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ntp_tiles/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace omnibox_everywhere::prefs {
namespace {

class OmniboxEverywherePrefsTest : public testing::Test {
 public:
  OmniboxEverywherePrefsTest() {
    RegisterLocalStatePrefs(local_state_.registry());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_FallbackToNtpTrue) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_FallbackToNtpFalse) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ExplicitlyEnabled) {
  // Even if NTP shortcuts are disabled, explicit enabled overrides it.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));

  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_ExplicitlyDisabled) {
  // Even if NTP shortcuts are enabled, explicit disabled overrides it.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  local_state_.SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));

  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_NullLocalStateFallback) {
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, nullptr));

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, false);
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, nullptr));
}

TEST_F(OmniboxEverywherePrefsTest, ShortcutsVisible_NullProfile) {
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(nullptr, &local_state_));

  local_state_.SetInteger(
      kOmniboxEverywhereShowShortcuts,
      std::to_underlying(ShowShortcutsPrefValue::kDisabled));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(nullptr, &local_state_));

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(nullptr, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyBothCheckboxesDisabled) {
  // Populate enterprise shortcuts policy list.
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  // Top toggle is ON, but both enterprise and personal shortcuts are OFF.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  // Even if fallback or explicitly enabled, should return false because no
  // shortcuts exist to show.
  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_FALSE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyOnlyPersonalEnabled) {
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  false);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  true);

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_EnterprisePolicyOnlyEnterpriseEnabled) {
  {
    ScopedListPrefUpdate update(
        profile_.GetPrefs(), ntp_tiles::prefs::kEnterpriseShortcutsPolicyList);
    update->Append("https://corp.example.com");
  }

  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpEnterpriseShortcutsVisible,
                                  true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

TEST_F(OmniboxEverywherePrefsTest,
       ShortcutsVisible_WithoutEnterprisePolicyShortcutsAlwaysAvailable) {
  // Top toggle is ON, no enterprise policy list.
  // Checkboxes do not appear in Customize Chrome without enterprise policy,
  // so shortcuts remain available even if individual checkbox prefs are false.
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpShortcutsVisible, true);
  profile_.GetPrefs()->SetBoolean(ntp_prefs::kNtpPersonalShortcutsVisible,
                                  false);

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kUnset));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));

  local_state_.SetInteger(kOmniboxEverywhereShowShortcuts,
                          std::to_underlying(ShowShortcutsPrefValue::kEnabled));
  EXPECT_TRUE(IsOmniboxEverywhereShortcutsVisible(&profile_, &local_state_));
}

}  // namespace
}  // namespace omnibox_everywhere::prefs
