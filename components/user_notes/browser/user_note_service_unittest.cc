// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_service.h"

#include <vector>

#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_base_test.h"
#include "components/user_notes/browser/user_note_manager.h"
#include "components/user_notes/browser/user_note_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

class UserNoteServiceTest : public UserNoteBaseTest {
 protected:
  void SetUp() override {
    UserNoteBaseTest::SetUp();
    AddNewNotesToService(2);
  }
};

// Tests that note models are returned correctly by the service.
TEST_F(UserNoteServiceTest, GetNoteModel) {
  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 0u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 0u);

  // Getting existing note models should return the expected model.
  const UserNote* model1 = note_service_->GetNoteModel(note_ids_[0]);
  const UserNote* model2 = note_service_->GetNoteModel(note_ids_[1]);
  ASSERT_TRUE(model1);
  ASSERT_TRUE(model2);
  EXPECT_EQ(model1->id(), note_ids_[0]);
  EXPECT_EQ(model2->id(), note_ids_[1]);

  // Getting a note model that doesn't exist should return `nullptr` and not
  // crash.
  EXPECT_EQ(note_service_->GetNoteModel(base::UnguessableToken::Create()),
            nullptr);
}

// Tests that references to note managers are correctly added to the model map.
TEST_F(UserNoteServiceTest, OnNoteInstanceAddedToPage) {
  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 0u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 0u);

  // Simulate note instances being created in managers.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m1);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m2);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], m1);

  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
}

// Tests that references to note managers are correctly removed from the model
// map.
TEST_F(UserNoteServiceTest, OnNoteInstanceRemovedFromPage) {
  // Initial setup.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m1);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], m2);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], m1);

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Simulate a note instance being removed from a page. Its ref should be
  // removed from the model map, but only for the removed note.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[0], m1);
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Simulate the last instance of a note being removed from its page. Its model
  // should be cleaned up from the model map.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[0], m2);
  EXPECT_EQ(ModelMapSize(), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_FALSE(DoesModelExist(note_ids_[0]));

  // Repeat with the other note instance.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[1], m1);
  EXPECT_EQ(ModelMapSize(), 0u);
  EXPECT_FALSE(DoesModelExist(note_ids_[0]));
  EXPECT_FALSE(DoesModelExist(note_ids_[1]));
}

// Tests that partial notes are correctly identified as such.
TEST_F(UserNoteServiceTest, IsNoteInProgress) {
  EXPECT_EQ(CreationMapSize(), 0u);
  AddPartialNotesToService(2);
  EXPECT_EQ(CreationMapSize(), 2u);

  EXPECT_FALSE(note_service_->IsNoteInProgress(note_ids_[0]));
  EXPECT_FALSE(note_service_->IsNoteInProgress(note_ids_[1]));
  EXPECT_TRUE(note_service_->IsNoteInProgress(note_ids_[2]));
  EXPECT_TRUE(note_service_->IsNoteInProgress(note_ids_[3]));

  // The method should also return false for notes that don't exist.
  EXPECT_FALSE(
      note_service_->IsNoteInProgress(base::UnguessableToken::Create()));
}

// Tests that adding an instance of a partial note to a page does not impact
// the model map and the note manager references.
TEST_F(UserNoteServiceTest, AddPartialNoteInstance) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], manager);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], manager);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Create an in-progress note.
  EXPECT_EQ(CreationMapSize(), 0u);
  AddPartialNotesToService(1);
  EXPECT_EQ(CreationMapSize(), 1u);

  // Simulate the instance of the in-progress note being added to the note
  // manager.
  note_service_->OnNoteInstanceAddedToPage(note_ids_[2], manager);

  // Verify the model map hasn't been impacted and that the creation map is
  // still as expected.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(CreationMapSize(), 1u);
  EXPECT_TRUE(note_service_->IsNoteInProgress(note_ids_[2]));
}

// Tests that removing an instance of a partial note from a page does not impact
// the model map and the note manager references, and correctly clears the
// partial note from the creation map.
TEST_F(UserNoteServiceTest, RemovePartialNoteInstance) {
  // Initial setup.
  UserNoteManager* manager = ConfigureNewManager();
  note_service_->OnNoteInstanceAddedToPage(note_ids_[0], manager);
  note_service_->OnNoteInstanceAddedToPage(note_ids_[1], manager);

  // Verify initial setup.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);

  // Create an in-progress note.
  EXPECT_EQ(CreationMapSize(), 0u);
  AddPartialNotesToService(1);
  EXPECT_EQ(CreationMapSize(), 1u);

  // Simulate the instance of the in-progress note being removed from the note
  // manager.
  note_service_->OnNoteInstanceRemovedFromPage(note_ids_[2], manager);

  // Verify the model map hasn't been impacted and the partial note has been
  // removed from the creation map.
  EXPECT_EQ(ModelMapSize(), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(CreationMapSize(), 0u);
  EXPECT_FALSE(note_service_->IsNoteInProgress(note_ids_[2]));
}

}  // namespace user_notes
