// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_notes_manager.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "components/user_notes/browser/user_note_service.h"
#include "components/user_notes/user_notes_features.h"
#include "content/public/browser/page.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

namespace {

const std::string kNoteId1 = "note-id-1";
const std::string kNoteId2 = "note-id-2";
const std::string kNoteId3 = "note-id-3";

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

class UserNotesManagerTest : public testing::Test {
 public:
  UserNotesManagerTest() {
    scoped_feature_list_.InitAndEnableFeature(user_notes::kUserNotes);
    note_service_ = std::make_unique<UserNoteService>();
    UserNoteService::ModelMapEntry entry1(std::make_unique<UserNote>(kNoteId1));
    UserNoteService::ModelMapEntry entry2(std::make_unique<UserNote>(kNoteId2));
    UserNoteService::ModelMapEntry entry3(std::make_unique<UserNote>(kNoteId3));
    note_service_->model_map_.emplace(kNoteId1, std::move(entry1));
    note_service_->model_map_.emplace(kNoteId2, std::move(entry2));
    note_service_->model_map_.emplace(kNoteId3, std::move(entry3));
  }

  base::SafeRef<UserNote> GetSafeRefForNote(std::string id) {
    const auto& entry_it = note_service_->model_map_.find(id);
    EXPECT_NE(entry_it, note_service_->model_map_.end());
    return entry_it->second.model->GetSafeRef();
  }

  int ManagerCountForId(const std::string& id) {
    const auto& entry_it = note_service_->model_map_.find(id);
    if (entry_it == note_service_->model_map_.end()) {
      return -1;
    }
    return entry_it->second.managers.size();
  }

  int ModelMapSize() { return note_service_->model_map_.size(); }

  bool DoesManagerExistForId(const std::string& id, UserNotesManager* manager) {
    const auto& model_entry_it = note_service_->model_map_.find(id);
    if (model_entry_it == note_service_->model_map_.end()) {
      return false;
    }
    const auto& manager_entry_it =
        model_entry_it->second.managers.find(manager);
    return manager_entry_it != model_entry_it->second.managers.end();
  }

  bool DoResultsContainId(const std::vector<UserNoteInstance*>& instances,
                          const std::string& id) {
    bool found = false;
    for (UserNoteInstance* instance : instances) {
      if (instance->model().guid() == id) {
        found = true;
        break;
      }
    }
    return found;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<UserNoteService> note_service_;
};

TEST_F(UserNotesManagerTest, Destructor) {
  // Initial setup.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId1)));
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId3)));

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 1);
  EXPECT_EQ(m1->instance_map_.size(), 0u);
  EXPECT_EQ(m2->instance_map_.size(), 3u);

  // Destroy a manager with no instances. There should be no impact on the model
  // map.
  m1.reset();
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 1);

  // Destroy a manager with instances. Refs to the manager should be removed
  // from the model map for all notes. In this case, since this was also the
  // last ref for the test notes, the models will be removed from the model map.
  m2.reset();
  EXPECT_EQ(ModelMapSize(), 0);
}

TEST_F(UserNotesManagerTest, GetNoteInstance) {
  // Initial setup.
  auto m =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  m->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId1)));
  m->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));

  // Verify initial state.
  EXPECT_EQ(m->instance_map_.size(), 2u);

  // Try to get an instance that doesn't exist. There should be no crash.
  UserNoteInstance* i = m->GetNoteInstance(kNoteId3);
  EXPECT_EQ(i, nullptr);

  // Try to get an instance that exists. It should return the expected instance.
  m->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId3)));
  i = m->GetNoteInstance(kNoteId3);
  EXPECT_NE(i, nullptr);
  EXPECT_EQ(i->model().guid(), kNoteId3);
}

TEST_F(UserNotesManagerTest, GetAllNoteInstances) {
  // Initial setup.
  auto m =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());

  // Verify initial state.
  EXPECT_EQ(m->instance_map_.size(), 0u);

  // Try to get instances when there are none. It should return an empty vector.
  const auto& emptyResults = m->GetAllNoteInstances();
  EXPECT_EQ(emptyResults.size(), 0u);

  // Add a few instances to the manager and try to get them. All instances
  // should be returned.
  m->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId1)));
  m->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));
  m->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId3)));
  const auto& results = m->GetAllNoteInstances();
  EXPECT_EQ(results.size(), 3u);
  EXPECT_TRUE(DoResultsContainId(results, kNoteId1));
  EXPECT_TRUE(DoResultsContainId(results, kNoteId2));
  EXPECT_TRUE(DoResultsContainId(results, kNoteId3));
}

TEST_F(UserNotesManagerTest, RemoveNote) {
  // Initial setup.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  m1->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId1)));
  m1->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId1)));
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 0);
  EXPECT_EQ(m1->instance_map_.size(), 2u);
  EXPECT_EQ(m2->instance_map_.size(), 2u);

  // Remove a note instance from a manager. It should not affect the other
  // managers this note appears in, and it should not affect the other instances
  // for this manager.
  m1->RemoveNote(kNoteId1);
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 0);
  EXPECT_EQ(m1->instance_map_.size(), 1u);
  EXPECT_EQ(m2->instance_map_.size(), 2u);
  EXPECT_FALSE(DoesManagerExistForId(kNoteId1, m1.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId2, m1.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId1, m2.get()));

  // Remove the last instance of a manager. It should not cause a problem or
  // affect the other managers.
  m1->RemoveNote(kNoteId2);
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 0);
  EXPECT_EQ(m1->instance_map_.size(), 0u);
  EXPECT_EQ(m2->instance_map_.size(), 2u);
  EXPECT_FALSE(DoesManagerExistForId(kNoteId1, m1.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId2, m1.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId1, m2.get()));
}

TEST_F(UserNotesManagerTest, AddNoteInstance) {
  // Initial setup.
  auto m1 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());
  auto m2 =
      UserNotesManager::CreateForTest(NullPage(), note_service_->GetSafeRef());

  // Verify initial state.
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 0);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 0);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 0);
  EXPECT_EQ(m1->instance_map_.size(), 0u);
  EXPECT_EQ(m2->instance_map_.size(), 0u);

  // Add some note instances to a manager. It should be correctly reflected in
  // both the manager's instance map and the service's model map. It should not
  // affect other managers.
  m1->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId1)));
  m1->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 0);
  EXPECT_EQ(m1->instance_map_.size(), 2u);
  EXPECT_EQ(m2->instance_map_.size(), 0u);
  EXPECT_TRUE(DoesManagerExistForId(kNoteId1, m1.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId2, m1.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId3, m1.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId1, m2.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId2, m2.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId3, m2.get()));

  // Add instances to another manager. It should be correctly reflected in
  // both the manager's instance map and the service's model map. It should not
  // affect other managers or instances in other managers.
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId2)));
  m2->AddNoteInstance(
      std::make_unique<UserNoteInstance>(GetSafeRefForNote(kNoteId3)));
  EXPECT_EQ(ModelMapSize(), 3);
  EXPECT_EQ(ManagerCountForId(kNoteId1), 1);
  EXPECT_EQ(ManagerCountForId(kNoteId2), 2);
  EXPECT_EQ(ManagerCountForId(kNoteId3), 1);
  EXPECT_EQ(m1->instance_map_.size(), 2u);
  EXPECT_EQ(m2->instance_map_.size(), 2u);
  EXPECT_TRUE(DoesManagerExistForId(kNoteId1, m1.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId2, m1.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId3, m1.get()));
  EXPECT_FALSE(DoesManagerExistForId(kNoteId1, m2.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId2, m2.get()));
  EXPECT_TRUE(DoesManagerExistForId(kNoteId3, m2.get()));
}

}  // namespace user_notes
