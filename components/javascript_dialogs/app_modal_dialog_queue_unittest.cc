// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_queue.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace javascript_dialogs {
namespace {

class FakeView : public AppModalDialogView {
 public:
  void ShowAppModalDialog() override {}
  void ActivateAppModalDialog() override {}
  void CloseAppModalDialog() override {}
  void AcceptAppModalDialog() override {}
  void CancelAppModalDialog() override {}
  bool IsShowing() const override { return true; }
};

class TestDialogControllerBase : public AppModalDialogController {
 public:
  explicit TestDialogControllerBase(bool* was_destroyed = nullptr)
      : AppModalDialogController(nullptr,
                                 &extra_data_,
                                 u"test",
                                 content::JAVASCRIPT_DIALOG_TYPE_ALERT,
                                 u"msg",
                                 u"",
                                 false,
                                 false,
                                 false,
                                 base::DoNothing()),
        was_destroyed_(was_destroyed) {}

  ~TestDialogControllerBase() override {
    if (was_destroyed_) {
      *was_destroyed_ = true;
    }
  }

 private:
  ExtraDataMap extra_data_;
  raw_ptr<bool> was_destroyed_ = nullptr;
};

// A dialog controller whose ShowModalDialog() sets view_ to a fake,
// allowing CompleteDialog() to trigger ShowNextDialog() on destruction.
class TestDialogController : public TestDialogControllerBase {
 public:
  explicit TestDialogController(bool* was_shown = nullptr,
                                bool* was_destroyed = nullptr)
      : TestDialogControllerBase(was_destroyed), was_shown_(was_shown) {}

  void ShowModalDialog() override {
    if (was_shown_) {
      *was_shown_ = true;
    }
    view_ = &fake_view_;
  }

  void TriggerComplete() { CompleteDialog(); }

 private:
  raw_ptr<bool> was_shown_ = nullptr;
  FakeView fake_view_;
};

// A dialog controller that fails the test if shown. Used to verify that
// CancelAllDialogs() prevents queued dialogs from being displayed.
class FailIfShownDialogController : public TestDialogControllerBase {
 public:
  using TestDialogControllerBase::TestDialogControllerBase;

  void ShowModalDialog() override {
    ADD_FAILURE() << "Dialog must not be shown after CancelAllDialogs()";
  }
};

class AppModalDialogQueueTest : public testing::Test {
 protected:
  void TearDown() override {
    AppModalDialogQueue::GetInstance()->ResetForTesting();
  }
};

// Verifies that CancelAllDialogs() prevents queued dialogs from being shown
// when the active dialog completes during shutdown.
TEST_F(AppModalDialogQueueTest, CancelAllDialogsPreventsShowDuringShutdown) {
  auto* queue = AppModalDialogQueue::GetInstance();

  bool dialog1_shown = false;
  bool dialog2_destroyed = false;

  auto* dialog1 = new TestDialogController(&dialog1_shown);
  queue->AddDialog(dialog1);
  ASSERT_TRUE(dialog1_shown);
  ASSERT_TRUE(queue->HasActiveDialog());

  auto* dialog2 = new FailIfShownDialogController(&dialog2_destroyed);
  queue->AddDialog(dialog2);
  ASSERT_FALSE(dialog2_destroyed);

  // Without this call, dialog1->TriggerComplete() would show dialog2.
  queue->CancelAllDialogs();
  EXPECT_TRUE(dialog2_destroyed);

  dialog1->TriggerComplete();
  EXPECT_FALSE(queue->HasActiveDialog());

  dialog1->Invalidate();
  delete dialog1;
}

TEST_F(AppModalDialogQueueTest, AddDialogDeletesAfterShutdown) {
  auto* queue = AppModalDialogQueue::GetInstance();

  queue->CancelAllDialogs();

  bool destroyed = false;
  auto* dialog = new FailIfShownDialogController(&destroyed);
  queue->AddDialog(dialog);

  EXPECT_TRUE(destroyed);
  EXPECT_FALSE(queue->HasActiveDialog());
}

TEST_F(AppModalDialogQueueTest, ShowNextDialogDrainsQueueAfterShutdown) {
  auto* queue = AppModalDialogQueue::GetInstance();

  bool dialog1_shown = false;
  auto* dialog1 = new TestDialogController(&dialog1_shown);
  queue->AddDialog(dialog1);
  ASSERT_TRUE(dialog1_shown);

  bool destroyed2 = false;
  bool destroyed3 = false;
  queue->AddDialog(new FailIfShownDialogController(&destroyed2));
  queue->AddDialog(new FailIfShownDialogController(&destroyed3));

  queue->CancelAllDialogs();
  EXPECT_TRUE(destroyed2);
  EXPECT_TRUE(destroyed3);

  dialog1->TriggerComplete();
  EXPECT_FALSE(queue->HasActiveDialog());

  dialog1->Invalidate();
  delete dialog1;
}

}  // namespace
}  // namespace javascript_dialogs
