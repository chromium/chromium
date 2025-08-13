// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/session_restore_infobar/session_restore_infobar_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace session_restore_infobar {

class MockSessionRestoreInfobarModel : public SessionRestoreInfobarModel {
 public:
  explicit MockSessionRestoreInfobarModel(PrefService& prefs)
      : SessionRestoreInfobarModel(prefs) {}

  MOCK_METHOD(SessionRestoreMessageValue,
              GetSessionRestoreMessageValue,
              (),
              ());
};

class SessionRestoreInfobarControllerBrowserTest : public InProcessBrowserTest {
 public:
  SessionRestoreInfobarControllerBrowserTest() = default;
  ~SessionRestoreInfobarControllerBrowserTest() override = default;

  SessionRestoreInfobarControllerBrowserTest(
      const SessionRestoreInfobarControllerBrowserTest&) = delete;
  SessionRestoreInfobarControllerBrowserTest& operator=(
      const SessionRestoreInfobarControllerBrowserTest&) = delete;
};

// Test that the session restore infobar controller correctly determines
// whether the infobar can be shown based on the session restore status.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       CanShowInfobar_SessionRestoreStatusAndMessageValue) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);
  auto mock_model = std::make_unique<MockSessionRestoreInfobarModel>(
      *browser()->profile()->GetPrefs());
  MockSessionRestoreInfobarModel* raw_mock_model = mock_model.get();

  auto controller = std::make_unique<SessionRestoreInfobarController>();

  EXPECT_CALL(*raw_mock_model, GetSessionRestoreMessageValue())
      .WillOnce(
          testing::Return(SessionRestoreInfobarModel::
                              SessionRestoreMessageValue::OpenSpecificPages));
  EXPECT_FALSE(controller->CanShowInfobar(
      raw_mock_model->GetSessionRestoreMessageValue(), browser()->profile()));
}

// Test that the session restore infobar controller correctly determines
// whether the infobar should be shown based on current profile preferences.
IN_PROC_BROWSER_TEST_F(SessionRestoreInfobarControllerBrowserTest,
                       ShouldShowSessionRestoreInfobarOnStartup_MessageValue) {
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 4);

  // Infobar will not be shown as the preference is set to open specific pages.
  auto controller = std::make_unique<SessionRestoreInfobarController>();
  EXPECT_FALSE(controller->ShouldShowSessionRestoreInfobarOnStartup());

  // Infobar will be shown as the preference is set to continue where left off.
  browser()->profile()->GetPrefs()->SetInteger(prefs::kRestoreOnStartup, 1);
  EXPECT_TRUE(controller->ShouldShowSessionRestoreInfobarOnStartup());
}

}  // namespace session_restore_infobar
