// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_instance.h"

#include <memory>
#include <vector>

#include "base/memory/safe_ref.h"
#include "base/test/bind.h"
#include "base/unguessable_token.h"
#include "components/user_notes/browser/user_note_base_test.h"
#include "components/user_notes/model/user_note.h"
#include "components/user_notes/model/user_note_model_test_utils.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

using testing::_;
using testing::Invoke;
using testing::Mock;

namespace user_notes {

namespace {

// Partially mock the object under test so calls to
// `InitializeHighlightInternal` can be intercepted to prevent side effects.
class MockUserNoteInstance : public UserNoteInstance {
 public:
  explicit MockUserNoteInstance(base::SafeRef<UserNote> model_ref,
                                UserNoteManager* manager,
                                const gfx::Rect& simulate_attach_rect)
      : UserNoteInstance(model_ref, manager),
        attach_rect_(simulate_attach_rect) {}

  const gfx::Rect& attach_rect() { return attach_rect_; }

  MOCK_METHOD(void, InitializeHighlightInternal, (), (override));

  void MockInitializeHighlightInternal() { DidFinishAttachment(attach_rect_); }

 private:
  gfx::Rect attach_rect_;
};

}  // namespace

class UserNoteInstanceTest : public UserNoteBaseTest {
 public:
  void AddNewNoteToService(UserNoteTarget::TargetType type) {
    base::UnguessableToken id = base::UnguessableToken::Create();
    note_ids_.emplace_back(id);

    // Original text, target URL and selector are not used in these tests.
    auto target = std::make_unique<UserNoteTarget>(
        type, /*original_text=*/u"", GURL("https://www.example.com/"),
        /*selector=*/"");
    UserNoteService::ModelMapEntry entry(
        std::make_unique<UserNote>(id, GetTestUserNoteMetadata(),
                                   GetTestUserNoteBody(), std::move(target)));
    note_service_->model_map_.emplace(id, std::move(entry));
  }

  std::unique_ptr<MockUserNoteInstance> CreateInstanceForId(
      base::UnguessableToken note_id,
      UserNoteManager* manager,
      const gfx::Rect& simulate_attach_rect) {
    const auto& entry_it = note_service_->model_map_.find(note_id);
    return std::make_unique<MockUserNoteInstance>(
        entry_it->second.model->GetSafeRef(), manager, simulate_attach_rect);
  }

  bool IsAttachmentFinished(UserNoteInstance* instance) {
    return instance->finished_attachment_;
  }
};

// Tests that async highlight initialization is skipped for page-level notes.
TEST_F(UserNoteInstanceTest, InitializeHighlightSkipForPageLevelNote) {
  AddNewNoteToService(UserNoteTarget::TargetType::kPage);
  UserNoteManager* manager = ConfigureNewManager();

  // Page-level notes should expect an empty rect.
  gfx::Rect empty_rect;
  std::unique_ptr<MockUserNoteInstance> instance =
      CreateInstanceForId(note_ids_[0], manager, empty_rect);

  EXPECT_CALL(*instance, InitializeHighlightInternal).Times(0);

  bool callback_called = false;
  instance->InitializeHighlightIfNeeded(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // The mocks ensure the callback is invoked synchronously, so verifications
  // can happen immediately.
  EXPECT_TRUE(callback_called);
  EXPECT_FALSE(IsAttachmentFinished(instance.get()));
  EXPECT_EQ(instance->rect(), empty_rect);
}

// Tests that async highlight initialization works as expected for text notes.
TEST_F(UserNoteInstanceTest, InitializeHighlightTextNote) {
  AddNewNoteToService(UserNoteTarget::TargetType::kPageText);
  UserNoteManager* manager = ConfigureNewManager();

  gfx::Rect rect(10, 15, 100, 50);
  std::unique_ptr<MockUserNoteInstance> instance =
      CreateInstanceForId(note_ids_[0], manager, rect);

  EXPECT_CALL(*instance, InitializeHighlightInternal)
      .Times(1)
      .WillOnce(Invoke(instance.get(),
                       &MockUserNoteInstance::MockInitializeHighlightInternal));

  bool callback_called = false;
  instance->InitializeHighlightIfNeeded(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  // The mocks ensure the callback is invoked synchronously, so verifications
  // can happen immediately.
  EXPECT_TRUE(callback_called);
  EXPECT_TRUE(IsAttachmentFinished(instance.get()));
  EXPECT_EQ(instance->rect(), rect);
}

}  // namespace user_notes
