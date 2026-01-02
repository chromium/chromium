// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/privacy_sandbox/tracking_protection_settings.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/testing_pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

// Rollback does not apply to iOS.
#if !BUILDFLAG(IS_IOS)

class MaybeSetRollbackPrefsModeBTest : public testing::Test {
 public:
  MaybeSetRollbackPrefsModeBTest() {
    content_settings::CookieSettings::RegisterProfilePrefs(prefs()->registry());
    RegisterProfilePrefs(prefs()->registry());
  }

  void Initialize3pcdState(content_settings::CookieControlsMode cookies_mode,
                           bool all_3pcs_blocked) {
    prefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
    prefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled, all_3pcs_blocked);
    prefs()->SetInteger(prefs::kCookieControlsMode,
                        static_cast<int>(cookies_mode));
  }

  void VerifyRollbackState(content_settings::CookieControlsMode cookies_mode,
                           bool show_rollback_ui) {
    EXPECT_FALSE(prefs()->GetBoolean(prefs::kTrackingProtection3pcdEnabled));
    EXPECT_EQ(prefs()->GetBoolean(prefs::kShowRollbackUiModeB),
              show_rollback_ui);
    EXPECT_EQ(prefs()->GetInteger(prefs::kCookieControlsMode),
              static_cast<int>(cookies_mode));
    histogram_tester_.ExpectUniqueSample(
        "Privacy.3PCD.RollbackNotice.ShouldShow", show_rollback_ui, 1);
  }

  sync_preferences::TestingPrefServiceSyncable* prefs() { return &prefs_; }

 private:
  sync_preferences::TestingPrefServiceSyncable prefs_;
  base::HistogramTester histogram_tester_;
};

TEST_F(MaybeSetRollbackPrefsModeBTest, ShowsNoticeWhen3pcsAllowed) {
  Initialize3pcdState(content_settings::CookieControlsMode::kOff, false);
  MaybeSetRollbackPrefsModeB(prefs());
  VerifyRollbackState(content_settings::CookieControlsMode::kOff, true);
}

TEST_F(MaybeSetRollbackPrefsModeBTest,
       Blocks3pcsAndDoesNotShowNoticeWhen3pcsBlockedIn3pcd) {
  Initialize3pcdState(content_settings::CookieControlsMode::kOff, true);
  MaybeSetRollbackPrefsModeB(prefs());
  VerifyRollbackState(content_settings::CookieControlsMode::kBlockThirdParty,
                      false);
}

TEST_F(MaybeSetRollbackPrefsModeBTest, DoesNotShowNoticeWhen3pcsBlocked) {
  Initialize3pcdState(content_settings::CookieControlsMode::kBlockThirdParty,
                      false);
  MaybeSetRollbackPrefsModeB(prefs());
  VerifyRollbackState(content_settings::CookieControlsMode::kBlockThirdParty,
                      false);
}

#endif

}  // namespace
}  // namespace privacy_sandbox
