// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/user_notes/browser/user_notes_manager.h"
#include "components/user_notes/model/user_note_model_test_utils.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/page.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

namespace {

// This is a hack to use a null |Page| in the tests instead of creating a
// mock, which is a relatively high effort task. Passing |nullptr| to this
// function and using the return value in a constructor requiring a |Page&|
// will satisfy the compiler. Attempting to use the return value will cause a
// crash.
content::Page& CreatePageNullRef(content::Page* page_null_ptr) {
  return *page_null_ptr;
}

// A shortcut for calling |CreatePageNullRef| above.
content::Page& NullPage() {
  return CreatePageNullRef(nullptr);
}

class UserNoteServiceDelegateMockImpl : public UserNoteServiceDelegate {
 public:
  std::vector<content::WebContents*> GetAllWebContents() override {
    return std::vector<content::WebContents*>();
  }

  UserNotesUI* GetUICoordinatorForWebContents(
      const content::WebContents* wc) override {
    return nullptr;
  }
};

}  // namespace

class UserNoteServiceTest : public testing::Test {
 public:
  UserNoteServiceTest() {
    // Create 2 note ids.
    note_ids_.push_back(base::UnguessableToken::Create());
    note_ids_.push_back(base::UnguessableToken::Create());

    scoped_feature_list_.InitAndEnableFeature(user_notes::kUserNotes);
    note_service_ = std::make_unique<UserNoteService>(
        std::make_unique<UserNoteServiceDelegateMockImpl>());
    auto note1 = std::make_unique<UserNote>(
        note_ids_[0], GetTestUserNoteMetadata(), GetTestUserNoteBody(),
        GetTestUserNotePageTarget());
    auto note2 = std::make_unique<UserNote>(
        note_ids_[1], GetTestUserNoteMetadata(), GetTestUserNoteBody(),
        GetTestUserNotePageTarget());
    UserNoteService::ModelMapEntry entry1(std::move(note1));
    UserNoteService::ModelMapEntry entry2(std::move(note2));
    note_service_->model_map_.emplace(note_ids_[0], std::move(entry1));
    note_service_->model_map_.emplace(note_ids_[1], std::move(entry2));
  }

  int ManagerCountForId(const base::UnguessableToken& id) {
    const auto& entry_it = note_service_->model_map_.find(id);
    if (entry_it == note_service_->model_map_.end()) {
      return -1;
    }
    return entry_it->second.managers.size();
  }

  bool DoesModelExist(const base::UnguessableToken& id) {
    const auto& entry_it = note_service_->model_map_.find(id);
    return entry_it != note_service_->model_map_.end();
  }

  int ModelMapSize() { return note_service_->model_map_.size(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UserNoteService> note_service_;
  std::vector<base::UnguessableToken> note_ids_;
};

TEST_F(UserNoteServiceTest, AddNoteIntancesToModelMap) {
  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 0);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 0);

  // Simulate note instances being created in managers.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m1.get());
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m2.get());
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], m1.get());

  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1);
}

TEST_F(UserNoteServiceTest, RemoveNoteIntancesFromModelMap) {
  // Initial setup.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m1.get());
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m2.get());
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], m1.get());

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1);

  // Simulate a note instance being removed from a page. Its ref should be
  // removed from the model map, but only for the removed note.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[0], m1.get());
  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1);

  // Simulate the last instance of a note being removed from its page. Its model
  // should be cleaned up from the model map.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[0], m2.get());
  EXPECT_EQ(ModelMapSize(), 1);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1);
  EXPECT_FALSE(DoesModelExist(note_ids_[0]));

  // Repeat with the other note instance.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[1], m1.get());
  EXPECT_EQ(ModelMapSize(), 0);
  EXPECT_FALSE(DoesModelExist(note_ids_[0]));
  EXPECT_FALSE(DoesModelExist(note_ids_[1]));
}

}  // namespace user_notes
