// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"
#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/model/user_note_model_test_utils.h"
#include "components/user_notes/user_notes_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/scroll_view.h"

class UserNoteUICoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures(
        {user_notes::kUserNotes, features::kUnifiedSidePanel}, {});

    TestWithBrowserView::SetUp();

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));

    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

    user_note_ui_coordinator_ =
        UserNoteUICoordinator::GetOrCreateForBrowser(browser_view()->browser());
    service_ = user_notes::UserNoteServiceFactory::GetForContext(
        browser_view()->GetActiveWebContents()->GetBrowserContext());

    content::WebContents* active_contents =
        browser_view()->GetActiveWebContents();
    auto* registry = SidePanelRegistry::Get(active_contents);
    user_note_ui_coordinator_->CreateAndRegisterEntry(registry);

    coordinator_ = browser_view()->side_panel_coordinator();

    // Verify the first tab has one entry, kUserNote.
    active_contents = browser_view()->GetActiveWebContents();
    SidePanelRegistry* contextual_registry =
        SidePanelRegistry::Get(active_contents);
    EXPECT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->id(),
              SidePanelEntry::Id::kUserNote);
  }

  std::unique_ptr<user_notes::UserNote> CreateUserNote(
      base::UnguessableToken id) {
    return std::make_unique<user_notes::UserNote>(
        id, user_notes::GetTestUserNoteMetadata(),
        user_notes::GetTestUserNoteBody(),
        user_notes::GetTestUserNotePageTarget());
  }

 protected:
  raw_ptr<SidePanelCoordinator> coordinator_;
  raw_ptr<user_notes::UserNoteService> service_;
  raw_ptr<UserNoteUICoordinator> user_note_ui_coordinator_;
};

TEST_F(UserNoteUICoordinatorTest, ShowEmptyUserNoteSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->right_aligned_side_panel()->GetVisible());

  coordinator_->Show(SidePanelEntry::Id::kUserNote);
  user_note_ui_coordinator_->Invalidate();

  SidePanelEntry::Id entry_id = coordinator_->GetLastActiveEntryId().value();
  EXPECT_EQ(entry_id, SidePanelEntry::Id::kUserNote);

  constexpr int kSidePanelContentWrapperViewId = 43;
  views::View* user_note_coordinator_view =
      coordinator_->GetContentView()
          ->GetViewByID(kSidePanelContentWrapperViewId)
          ->GetViewByID(UserNoteUICoordinator::kUserNoteUIViewId);
  EXPECT_TRUE(user_note_coordinator_view);
  views::ScrollView* user_note_scroll_view =
      static_cast<views::ScrollView*>(user_note_coordinator_view->GetViewByID(
          UserNoteUICoordinator::kUserNoteScrollViewId));
  EXPECT_TRUE(user_note_scroll_view);
  EXPECT_TRUE(user_note_scroll_view->contents());
  EXPECT_EQ(user_note_scroll_view->contents()->children().size(), 0u);
}
