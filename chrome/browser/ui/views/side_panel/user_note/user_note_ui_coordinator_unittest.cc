// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"
#include "chrome/browser/ui/views/side_panel/user_note/user_note_view.h"
#include "chrome/browser/user_notes/user_note_service_factory.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/interfaces/user_note_storage.h"
#include "components/user_notes/model/user_note_model_test_utils.h"
#include "components/user_notes/user_notes_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/view_utils.h"

using user_notes::UserNoteInstance;

namespace {

// Mock the note storage to prevent side effects.
class MockUserNoteStorage : public user_notes::UserNoteStorage {
 public:
  void GetNoteMetadataForUrls(
      const user_notes::UserNoteStorage::UrlSet& urls,
      base::OnceCallback<void(user_notes::UserNoteMetadataSnapshot)> callback)
      override {
    std::move(callback).Run(user_notes::UserNoteMetadataSnapshot());
  }

  void GetNotesById(const user_notes::UserNoteStorage::IdSet& ids,
                    base::OnceCallback<void(
                        std::vector<std::unique_ptr<user_notes::UserNote>>)>
                        callback) override {
    std::move(callback).Run(
        std::vector<std::unique_ptr<user_notes::UserNote>>());
  }

  // No-op.
  void UpdateNote(const user_notes::UserNote* model,
                  std::u16string note_body_text,
                  bool is_creation = false) override {}

  // No-op.
  void DeleteNote(const base::UnguessableToken& guid) override {}

  // No-op.
  void DeleteAllForUrl(const GURL& url) override {}

  // No-op.
  void DeleteAllForOrigin(const url::Origin& origin) override {}

  // No-op.
  void DeleteAllNotes() override {}

  // No-op.
  void AddObserver(Observer* observer) override {}

  // No-op.
  void RemoveObserver(Observer* observer) override {}
};

// Mock the note service to prevent side effects.
class MockUserNoteService : public user_notes::UserNoteService {
 public:
  MockUserNoteService()
      : UserNoteService(/*delegate=*/nullptr,
                        std::make_unique<MockUserNoteStorage>()) {}

  // No-op.
  void OnFrameNavigated(content::RenderFrameHost* rfh) override {}
};

}  // namespace

class UserNoteUICoordinatorTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {user_notes::kUserNotes, features::kUnifiedSidePanel}, {});

    TestWithBrowserView::SetUp();

    user_notes::UserNoteServiceFactory::SetServiceForTesting(
        std::make_unique<MockUserNoteService>());
    service_ = user_notes::UserNoteServiceFactory::GetForContext(
        browser_view()->GetProfile());
    user_note_ui_coordinator_ =
        UserNoteUICoordinator::GetOrCreateForBrowser(browser_view()->browser());

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

    content::WebContents* active_contents =
        browser_view()->GetActiveWebContents();
    auto* registry = SidePanelRegistry::Get(active_contents);
    user_note_ui_coordinator_->CreateAndRegisterEntry(registry);

    coordinator_ = browser_view()->side_panel_coordinator();
    coordinator_->SetNoDelaysForTesting();

    // Verify the first tab has one entry, kUserNote.
    active_contents = browser_view()->GetActiveWebContents();
    SidePanelRegistry* contextual_registry =
        SidePanelRegistry::Get(active_contents);
    EXPECT_EQ(contextual_registry->entries().size(), 1u);
    EXPECT_EQ(contextual_registry->entries()[0]->key().id(),
              SidePanelEntry::Id::kUserNote);
  }

  std::unique_ptr<user_notes::UserNote> CreateUserNote(
      base::UnguessableToken id) {
    return std::make_unique<user_notes::UserNote>(
        id, user_notes::GetTestUserNoteMetadata(),
        user_notes::GetTestUserNoteBody(),
        user_notes::GetTestUserNotePageTarget());
  }

  void AddUserNote(user_notes::UserNoteManager* manager, size_t index) {
    auto note = CreateUserNote(note_ids_[index]);
    auto safe_ref = note->GetSafeRef();
    service_->model_map_.emplace(note_ids_[index], std::move(note));

    std::unique_ptr<UserNoteInstance> note_instance =
        UserNoteInstance::Create(safe_ref, manager);
    auto* instance_raw = note_instance.get();

    manager->AddNoteInstance(std::move(note_instance));

    instance_raw->DidFinishAttachment(note_rects_[index]);
  }

  views::ScrollView* GetUserNoteScrollView() {
    auto* scroll_view = views::AsViewClass<views::ScrollView>(
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            UserNoteUICoordinator::kScrollViewElementIdForTesting,
            browser_view()->GetElementContext()));
    EXPECT_TRUE(scroll_view);
    EXPECT_TRUE(scroll_view->contents());
    return scroll_view;
  }

 protected:
  raw_ptr<SidePanelCoordinator> coordinator_;
  raw_ptr<user_notes::UserNoteService> service_;
  raw_ptr<UserNoteUICoordinator> user_note_ui_coordinator_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::vector<base::UnguessableToken> note_ids_;
  std::vector<gfx::Rect> note_rects_;
};

TEST_F(UserNoteUICoordinatorTest, ShowEmptyUserNoteSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  SidePanelEntry::Id entry_id =
      coordinator_->GetLastActiveEntryKey().value().id();
  EXPECT_EQ(entry_id, SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 0u);
}

TEST_F(UserNoteUICoordinatorTest, PopulateUserNoteSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,10)
  // note3 = rect(0,0,0,20)
  for (size_t i = 0; i < 3; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(gfx::Rect(0, 0, 0, 10 * i));
    AddUserNote(manager, i);
  }

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  size_t index = 0;
  for (auto* child_view : scroll_view->contents()->children()) {
    UserNoteView* user_note_view = views::AsViewClass<UserNoteView>(child_view);
    // Verify that the notes added to the service are displayed in the user note
    // side panel.
    EXPECT_EQ(user_note_view->user_note_id(), note_ids_[index]);
    index++;
  }
}

TEST_F(UserNoteUICoordinatorTest, AddNoteMiddleUserSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,10)
  for (size_t i = 0; i < 2; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(gfx::Rect(0, 0, 0, 10 * i));
    AddUserNote(manager, i);
  }

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 2u);

  // Add a note to the service:
  // note3 = rect(0,0,0,5)
  note_ids_.emplace_back(base::UnguessableToken::Create());
  note_rects_.emplace_back(gfx::Rect(0, 0, 0, 5));
  size_t index = note_ids_.size() - 1;
  AddUserNote(manager, index);

  user_note_ui_coordinator_->InvalidateIfVisible();
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  UserNoteView* middle_user_note_view = views::AsViewClass<UserNoteView>(
      scroll_view->contents()->children().at(1));
  // Verify that note3 is the middle note in the side panel.
  EXPECT_EQ(middle_user_note_view->user_note_id(), note_ids_[index]);
}

TEST_F(UserNoteUICoordinatorTest, AddNoteEndUserSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,1)
  // note3 = rect(0,0,0,2)
  for (size_t i = 0; i < 3; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(0, 0, 0, i);
    AddUserNote(manager, i);
  }

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  // Add a note to the service:
  // note3 = rect(0,0,0,3)
  note_ids_.emplace_back(base::UnguessableToken::Create());
  note_rects_.emplace_back(gfx::Rect(0, 0, 0, 3));
  size_t index = note_ids_.size() - 1;
  AddUserNote(manager, index);

  user_note_ui_coordinator_->InvalidateIfVisible();
  EXPECT_EQ(scroll_view->contents()->children().size(), 4u);

  UserNoteView* last_user_note_view = views::AsViewClass<UserNoteView>(
      scroll_view->contents()->children().at(index));
  // Verify that note3 is the last note in the side panel.
  EXPECT_EQ(last_user_note_view->user_note_id(), note_ids_[index]);
}

// TODO(crbug.com/1328966): Re-enable this test
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(ADDRESS_SANITIZER)
#define MAYBE_RemoveMiddleUserSidePanel DISABLED_RemoveMiddleUserSidePanel
#else
#define MAYBE_RemoveMiddleUserSidePanel RemoveMiddleUserSidePanel
#endif
TEST_F(UserNoteUICoordinatorTest, MAYBE_RemoveMiddleUserSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,1)
  // note3 = rect(0,0,0,2)
  for (size_t i = 0; i < 3; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(0, 0, 0, i);
    AddUserNote(manager, i);
  }

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  // Remove note2 from the service.
  manager->RemoveNote(note_ids_[1]);
  user_note_ui_coordinator_->InvalidateIfVisible();

  EXPECT_EQ(scroll_view->contents()->children().size(), 2u);

  for (auto* child_view : scroll_view->contents()->children()) {
    UserNoteView* user_note_view = views::AsViewClass<UserNoteView>(child_view);
    // Verify that note2 has been removed from the side panel.
    EXPECT_NE(user_note_view->user_note_id(), note_ids_[1]);
  }
}

// TODO(crbug.com/1328966): Re-enable this test
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(ADDRESS_SANITIZER)
#define MAYBE_RemoveEndUserSidePanel DISABLED_RemoveEndUserSidePanel
#else
#define MAYBE_RemoveEndUserSidePanel RemoveEndUserSidePanel
#endif
TEST_F(UserNoteUICoordinatorTest, MAYBE_RemoveEndUserSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,1)
  // note3 = rect(0,0,0,2)
  for (size_t i = 0; i < 3; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(0, 0, 0, i);
    AddUserNote(manager, i);
  }

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  // Remove note3 from the service.
  size_t index = note_ids_.size() - 1;
  manager->RemoveNote(note_ids_[index]);
  user_note_ui_coordinator_->InvalidateIfVisible();
  EXPECT_EQ(scroll_view->contents()->children().size(), 2u);

  for (auto* child_view : scroll_view->contents()->children()) {
    UserNoteView* user_note_view = views::AsViewClass<UserNoteView>(child_view);
    // Verify that note3 has been removed from the side panel.
    EXPECT_NE(user_note_view->user_note_id(), note_ids_[index]);
  }
}

// TODO(crbug.com/1328966): Re-enable this test
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(ADDRESS_SANITIZER)
#define MAYBE_RemoveAllNoteUserSidePanel DISABLED_RemoveAllNoteUserSidePanel
#else
#define MAYBE_RemoveAllNoteUserSidePanel RemoveAllNoteUserSidePanel
#endif
TEST_F(UserNoteUICoordinatorTest, MAYBE_RemoveAllNoteUserSidePanel) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,1)
  // note3 = rect(0,0,0,2)
  for (size_t i = 0; i < 3; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(0, 0, 0, i);
    AddUserNote(manager, i);
  }

  coordinator_->Show(SidePanelEntry::Id::kUserNote);

  auto* scroll_view = GetUserNoteScrollView();
  EXPECT_EQ(scroll_view->contents()->children().size(), 3u);

  // Remove all notes from the service.
  for (base::UnguessableToken id : note_ids_) {
    manager->RemoveNote(id);
  }

  user_note_ui_coordinator_->InvalidateIfVisible();
  // Verify that the user note side panel is empty.
  EXPECT_EQ(scroll_view->contents()->children().size(), 0u);
}

TEST_F(UserNoteUICoordinatorTest, CleanScrollViewOnSidePanelCloseWithoutNotes) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  coordinator_->Show(SidePanelEntry::Id::kUserNote);
  EXPECT_NE(user_note_ui_coordinator_->scroll_view_, nullptr);

  coordinator_->Close();
  EXPECT_EQ(user_note_ui_coordinator_->scroll_view_, nullptr);
}

TEST_F(UserNoteUICoordinatorTest, CleanScrollViewOnSidePanelCloseWithNotes) {
  coordinator_->Toggle();
  EXPECT_TRUE(browser_view()->unified_side_panel()->GetVisible());

  user_notes::UserNoteManager* manager =
      user_notes::UserNoteManager::GetForPage(
          browser_view()->GetActiveWebContents()->GetPrimaryPage());
  EXPECT_TRUE(manager);

  // Add 3 notes with the following rects:
  // note1 = rect(0,0,0,0)
  // note2 = rect(0,0,0,10)
  // note3 = rect(0,0,0,20)
  for (size_t i = 0; i < 3; ++i) {
    note_ids_.emplace_back(base::UnguessableToken::Create());
    note_rects_.emplace_back(0, 0, 0, i);
    AddUserNote(manager, i);
  }

  // Show user note from the user note coordinator
  user_note_ui_coordinator_->Show();
  EXPECT_NE(user_note_ui_coordinator_->scroll_view_, nullptr);

  coordinator_->Close();
  EXPECT_EQ(user_note_ui_coordinator_->scroll_view_, nullptr);
}
