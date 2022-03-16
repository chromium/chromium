// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/user_notes/browser/user_notes_manager.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/page.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

namespace {

const std::string kNoteId1 = "note-id-1";
const std::string kNoteId2 = "note-id-2";

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

}  // namespace

class UserNoteServiceTest : public testing::Test {
 public:
  UserNoteServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(user_notes::kUserNotes);
    note_service_ = std::make_unique<UserNoteService>();
    auto note1 = std::make_unique<UserNote>(kNoteId1);
    auto note2 = std::make_unique<UserNote>(kNoteId2);
    UserNoteService::ModelMapEntry entry1(std::move(note1));
    UserNoteService::ModelMapEntry entry2(std::move(note2));
    note_service_->model_map_.emplace(kNoteId1, std::move(entry1));
    note_service_->model_map_.emplace(kNoteId2, std::move(entry2));
  }

  int ManagerCountForId(const std::string& id) {
    const auto& entry_it = note_service_->model_map_.find(id);
    if (entry_it == note_service_->model_map_.end()) {
      return -1;
    }
    return entry_it->second.managers.size();
  }

  bool DoesModelExist(const std::string& id) {
    const auto& entry_it = note_service_->model_map_.find(id);
    return entry_it != note_service_->model_map_.end();
  }

  int ModelMapSize() { return note_service_->model_map_.size(); }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UserNoteService> note_service_;
};

TEST_F(UserNoteServiceTest, AddNoteIntancesToModelMap) {
  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 0);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 0);

  // Simulate note instances being created in managers.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  note_service_->OnNoteInstanceAddedToPage(kNoteId1, m1.get());
  note_service_->OnNoteInstanceAddedToPage(kNoteId1, m2.get());
  note_service_->OnNoteInstanceAddedToPage(kNoteId2, m1.get());

  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);
}

TEST_F(UserNoteServiceTest, RemoveNoteIntancesFromModelMap) {
  // Initial setup.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  note_service_->OnNoteInstanceAddedToPage(kNoteId1, m1.get());
  note_service_->OnNoteInstanceAddedToPage(kNoteId1, m2.get());
  note_service_->OnNoteInstanceAddedToPage(kNoteId2, m1.get());

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);

  // Simulate a note instance being removed from a page. Its ref should be
  // removed from the model map, but only for the removed note.
  note_service_->OnNoteInstanceRemovedFromPage(kNoteId1, m1.get());
  EXPECT_EQ(ModelMapSize(), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);

  // Simulate the last instance of a note being removed from its page. Its model
  // should be cleaned up from the model map.
  note_service_->OnNoteInstanceRemovedFromPage(kNoteId1, m2.get());
  EXPECT_EQ(ModelMapSize(), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);
  EXPECT_FALSE(DoesModelExist(kNoteId1));

  // Repeat with the other note instance.
  note_service_->OnNoteInstanceRemovedFromPage(kNoteId2, m1.get());
  EXPECT_EQ(ModelMapSize(), 0);
  EXPECT_FALSE(DoesModelExist(kNoteId1));
  EXPECT_FALSE(DoesModelExist(kNoteId2));
}

}  // namespace user_notes
