// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/contextual_tasks/contextual_tasks_ephemeral_button_utils.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_id.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {
namespace {

class ContextualTasksEphemeralButtonUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    ON_CALL(browser_, GetProfile())
        .WillByDefault(testing::Return(profile_.get()));
  }

 protected:
  // Sets the contextual tasks side panel alignment through its per-entry
  // override, which takes precedence over the global alignment pref.
  void SetSidePanelRightAligned(bool right_aligned) {
    ScopedDictPrefUpdate update(profile_->GetPrefs(),
                                prefs::kSidePanelAlignmentOverrides);
    update->Set(SidePanelEntryIdToString(SidePanelEntryId::kContextualTasks),
                right_aligned);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  testing::NiceMock<MockBrowserWindowInterface> browser_;
};

TEST_F(ContextualTasksEphemeralButtonUtilsTest,
       FallsBackToGlobalAlignmentWithoutOverride) {
  profile_->GetPrefs()->SetDict(prefs::kSidePanelAlignmentOverrides,
                                base::DictValue());

  profile_->GetPrefs()->SetBoolean(prefs::kSidePanelHorizontalAlignment, true);
  EXPECT_TRUE(IsContextualTasksSidePanelRightAligned(&browser_));

  profile_->GetPrefs()->SetBoolean(prefs::kSidePanelHorizontalAlignment, false);
  EXPECT_FALSE(IsContextualTasksSidePanelRightAligned(&browser_));

  // The contextual tasks override takes precedence over the global pref.
  SetSidePanelRightAligned(true);
  EXPECT_TRUE(IsContextualTasksSidePanelRightAligned(&browser_));
}

}  // namespace
}  // namespace contextual_tasks
