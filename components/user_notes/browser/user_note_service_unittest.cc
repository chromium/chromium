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

TEST_F(UserNoteServiceTest, AddNoteIntancesToModelMap) {
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

TEST_F(UserNoteServiceTest, RemoveNoteIntancesFromModelMap) {
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

}  // namespace user_notes
