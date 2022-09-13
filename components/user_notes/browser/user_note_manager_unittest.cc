// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_manager.h"

#include <vector>

#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_base_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

class UserNoteManagerTest : public UserNoteBaseTest {
 protected:
  void SetUp() override {
    UserNoteBaseTest::SetUp();
    AddNewNotesToService(3);
  }

  bool DoResultsContainId(const std::vector<UserNoteInstance*>& instances,
                          const base::UnguessableToken& id) {
    bool found = false;
    for (UserNoteInstance* instance : instances) {
      if (instance->model().id() == id) {
        found = true;
        break;
      }
    }
    return found;
  }
};

TEST_F(UserNoteManagerTest, Destructor) {
  // Initial setup.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();
  AddNewInstanceToManager(m2, note_ids_[0]);
  AddNewInstanceToManager(m2, note_ids_[1]);
  AddNewInstanceToManager(m2, note_ids_[2]);

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(InstanceMapSize(m1), 0u);
  EXPECT_EQ(InstanceMapSize(m2), 3u);

  // Destroy a manager with no instances. There should be no impact on the model
  // map. To do that, destroy the corresponding `WebContents` stored in the
  // test, which will destroy the attached manager.
  web_contents_list_[0].reset();
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);

  // Destroy a manager with instances. Refs to the manager should be removed
  // from the model map for all notes. In this case, since this was also the
  // last ref for the test notes, the models will be removed from the model map.
  web_contents_list_[1].reset();
  EXPECT_EQ(ModelMapSize(), 0u);
}

TEST_F(UserNoteManagerTest, GetNoteInstance) {
  // Initial setup.
  UserNoteManager* m = ConfigureNewManager();
  AddNewInstanceToManager(m, note_ids_[0]);
  AddNewInstanceToManager(m, note_ids_[1]);

  // Verify initial state.
  EXPECT_EQ(InstanceMapSize(m), 2u);

  // Try to get an instance that doesn't exist. There should be no crash.
  UserNoteInstance* i = m->GetNoteInstance(note_ids_[2]);
  EXPECT_EQ(i, nullptr);

  // Try to get an instance that exists. It should return the expected instance.
  AddNewInstanceToManager(m, note_ids_[2]);
  i = m->GetNoteInstance(note_ids_[2]);
  EXPECT_NE(i, nullptr);
  EXPECT_EQ(i->model().id(), note_ids_[2]);
}

TEST_F(UserNoteManagerTest, GetAllNoteInstances) {
  // Initial setup.
  UserNoteManager* m = ConfigureNewManager();

  // Verify initial state.
  EXPECT_EQ(InstanceMapSize(m), 0u);

  // Try to get instances when there are none. It should return an empty vector.
  const auto& emptyResults = m->GetAllNoteInstances();
  EXPECT_EQ(emptyResults.size(), 0u);

  // Add a few instances to the manager and try to get them. All instances
  // should be returned.
  AddNewInstanceToManager(m, note_ids_[0]);
  AddNewInstanceToManager(m, note_ids_[1]);
  AddNewInstanceToManager(m, note_ids_[2]);

  EXPECT_EQ(InstanceMapSize(m), 3u);
  const auto& results = m->GetAllNoteInstances();
  EXPECT_EQ(results.size(), 3u);
  EXPECT_TRUE(DoResultsContainId(results, note_ids_[0]));
  EXPECT_TRUE(DoResultsContainId(results, note_ids_[1]));
  EXPECT_TRUE(DoResultsContainId(results, note_ids_[2]));
}

TEST_F(UserNoteManagerTest, RemoveNote) {
  // Initial setup.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();
  AddNewInstanceToManager(m1, note_ids_[0]);
  AddNewInstanceToManager(m1, note_ids_[1]);
  AddNewInstanceToManager(m2, note_ids_[0]);
  AddNewInstanceToManager(m2, note_ids_[1]);

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 0u);
  EXPECT_EQ(InstanceMapSize(m1), 2u);
  EXPECT_EQ(InstanceMapSize(m2), 2u);

  // Remove a note instance from a manager. It should not affect the other
  // managers this note appears in, and it should not affect the other instances
  // for this manager.
  m1->RemoveNote(note_ids_[0]);
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 0u);
  EXPECT_EQ(InstanceMapSize(m1), 1u);
  EXPECT_EQ(InstanceMapSize(m2), 2u);
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[0], m1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], m1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], m2));

  // Remove the last instance of a manager. It should not cause a problem or
  // affect the other managers.
  m1->RemoveNote(note_ids_[1]);
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 0u);
  EXPECT_EQ(InstanceMapSize(m1), 0u);
  EXPECT_EQ(InstanceMapSize(m2), 2u);
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[0], m1));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[1], m1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], m2));
}

TEST_F(UserNoteManagerTest, AddNoteInstance) {
  // Initial setup.
  UserNoteManager* m1 = ConfigureNewManager();
  UserNoteManager* m2 = ConfigureNewManager();

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 0u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 0u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 0u);
  EXPECT_EQ(InstanceMapSize(m1), 0u);
  EXPECT_EQ(InstanceMapSize(m2), 0u);

  // Add some note instances to a manager. It should be correctly reflected in
  // both the manager's instance map and the service's model map. It should not
  // affect other managers.
  AddNewInstanceToManager(m1, note_ids_[0]);
  AddNewInstanceToManager(m1, note_ids_[1]);
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 0u);
  EXPECT_EQ(InstanceMapSize(m1), 2u);
  EXPECT_EQ(InstanceMapSize(m2), 0u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], m1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], m1));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[2], m1));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[0], m2));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[1], m2));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[2], m2));

  // Add instances to another manager. It should be correctly reflected in
  // both the manager's instance map and the service's model map. It should not
  // affect other managers or instances in other managers.
  AddNewInstanceToManager(m2, note_ids_[1]);
  AddNewInstanceToManager(m2, note_ids_[2]);
  EXPECT_EQ(ModelMapSize(), 3u);
  EXPECT_EQ(ManagerCountForId(note_ids_[0]), 1u);
  EXPECT_EQ(ManagerCountForId(note_ids_[1]), 2u);
  EXPECT_EQ(ManagerCountForId(note_ids_[2]), 1u);
  EXPECT_EQ(InstanceMapSize(m1), 2u);
  EXPECT_EQ(InstanceMapSize(m2), 2u);
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[0], m1));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], m1));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[2], m1));
  EXPECT_FALSE(DoesManagerExistForId(note_ids_[0], m2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[1], m2));
  EXPECT_TRUE(DoesManagerExistForId(note_ids_[2], m2));
}

}  // namespace user_notes
