// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/frame_user_note_changes.h"

#include <memory>
#include <vector>

#include "base/memory/safe_ref.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_base_test.h"
#include "components/user_notes/browser/user_note_instance.h"
#include "components/user_notes/model/user_note.h"
#include "content/public/browser/render_frame_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

namespace user_notes {

namespace {

// A mock for a note instance that synchronously invokes the callback when
// initializing the text highlight.
class MockUserNoteInstance : public UserNoteInstance {
 public:
  explicit MockUserNoteInstance(base::SafeRef<UserNote> model_ref,
                                UserNoteManager* manager)
      : UserNoteInstance(model_ref, manager) {}

  MOCK_METHOD(void,
              InitializeHighlightIfNeeded,
              (UserNoteInstance::AttachmentFinishedCallback callback),
              (override));
};

// Partially mock the object under test so calls to `MakeNoteInstance` can be
// intercepted, allowing the tests to create mocked instances.
class MockFrameUserNoteChanges : public FrameUserNoteChanges {
 public:
  MockFrameUserNoteChanges(
      base::SafeRef<UserNoteService> service,
      content::WeakDocumentPtr document,
      const FrameUserNoteChanges::ChangeList& notes_added,
      const FrameUserNoteChanges::ChangeList& notes_modified,
      const FrameUserNoteChanges::ChangeList& notes_removed)
      : FrameUserNoteChanges(service,
                             document,
                             notes_added,
                             notes_modified,
                             notes_removed) {}

  MOCK_METHOD(std::unique_ptr<UserNoteInstance>,
              MakeNoteInstance,
              (const UserNote* note_model, UserNoteManager* manager),
              (const override));
};

void MockInitializeHighlightIfNeeded(base::OnceClosure callback) {
  std::move(callback).Run();
}

std::unique_ptr<UserNoteInstance> MockMakeNoteInstance(
    const UserNote* note_model,
    UserNoteManager* manager) {
  auto instance_mock =
      std::make_unique<MockUserNoteInstance>(note_model->GetSafeRef(), manager);

  EXPECT_CALL(*instance_mock, InitializeHighlightIfNeeded(_))
      .Times(1)
      .WillOnce(&MockInitializeHighlightIfNeeded);

  return instance_mock;
}

}  // namespace

class FrameUserNoteChangesTest : public UserNoteBaseTest {};

// Tests that added notes correctly kick off highlight initialization on the
// renderer side, and new instances are correctly added to the note manager.
TEST_F(FrameUserNoteChangesTest, ApplyAddedNotes) {
  AddNewNotesToService(3);
  UserNoteManager* m = ConfigureNewManager();
  AddNewInstanceToManager(m, note_ids_[0]);

  std::vector<base::UnguessableToken> added({note_ids_[1], note_ids_[2]});
  std::vector<base::UnguessableToken> modified;
  std::vector<base::UnguessableToken> removed;

  auto mock_changes = std::make_unique<MockFrameUserNoteChanges>(
      note_service_->GetSafeRef(),
      web_contents_list_[0]->GetPrimaryMainFrame()->GetWeakDocumentPtr(), added,
      modified, removed);

  EXPECT_CALL(*mock_changes, MakeNoteInstance(_, _))
      .Times(2)
      .WillRepeatedly(&MockMakeNoteInstance);

  bool callback_called = false;
  mock_changes->Apply(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // The mocks ensure the callback is invoked synchronously, so verifications
  // can happen immediately.
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(InstanceMapSize(m), 3u);
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[0]));
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[1]));
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[2]));
}

// Tests that modified notes don't impact instances in the note manager.
TEST_F(FrameUserNoteChangesTest, ApplyModifiedNotes) {
  AddNewNotesToService(3);
  UserNoteManager* m = ConfigureNewManager();
  AddNewInstanceToManager(m, note_ids_[0]);
  AddNewInstanceToManager(m, note_ids_[1]);
  AddNewInstanceToManager(m, note_ids_[2]);

  std::vector<base::UnguessableToken> added;
  std::vector<base::UnguessableToken> modified({note_ids_[0], note_ids_[2]});
  std::vector<base::UnguessableToken> removed;

  auto mock_changes = std::make_unique<MockFrameUserNoteChanges>(
      note_service_->GetSafeRef(),
      web_contents_list_[0]->GetPrimaryMainFrame()->GetWeakDocumentPtr(), added,
      modified, removed);

  EXPECT_CALL(*mock_changes, MakeNoteInstance(_, _)).Times(0);

  bool callback_called = false;
  mock_changes->Apply(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // The mocks ensure the callback is invoked synchronously, so verifications
  // can happen immediately.
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(InstanceMapSize(m), 3u);
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[0]));
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[1]));
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[2]));
}

// Tests that removed notes correctly have their instances removed from the
// note manager.
TEST_F(FrameUserNoteChangesTest, ApplyRemovedNotes) {
  AddNewNotesToService(3);
  UserNoteManager* m = ConfigureNewManager();
  AddNewInstanceToManager(m, note_ids_[0]);
  AddNewInstanceToManager(m, note_ids_[1]);
  AddNewInstanceToManager(m, note_ids_[2]);

  std::vector<base::UnguessableToken> added;
  std::vector<base::UnguessableToken> modified;
  std::vector<base::UnguessableToken> removed({note_ids_[0], note_ids_[2]});

  auto mock_changes = std::make_unique<MockFrameUserNoteChanges>(
      note_service_->GetSafeRef(),
      web_contents_list_[0]->GetPrimaryMainFrame()->GetWeakDocumentPtr(), added,
      modified, removed);

  EXPECT_CALL(*mock_changes, MakeNoteInstance(_, _)).Times(0);

  bool callback_called = false;
  mock_changes->Apply(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // The mocks ensure the callback is invoked synchronously, so verifications
  // can happen immediately.
  EXPECT_TRUE(callback_called);
  EXPECT_EQ(InstanceMapSize(m), 1u);
  EXPECT_TRUE(m->GetNoteInstance(note_ids_[1]));
  EXPECT_FALSE(m->GetNoteInstance(note_ids_[0]));
  EXPECT_FALSE(m->GetNoteInstance(note_ids_[2]));
}

}  // namespace user_notes
