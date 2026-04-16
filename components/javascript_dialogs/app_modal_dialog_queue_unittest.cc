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

// A view, a la what is returned by AppModalDialogManager::AppModalViewFactory,
// to own an AppModalDialogController.
class FakeView : public AppModalDialogView {
 public:
  explicit FakeView(std::unique_ptr<AppModalDialogController> controller)
      : controller_(std::move(controller)) {}

  void ShowAppModalDialog() override {}
  void ActivateAppModalDialog() override {}
  void CloseAppModalDialog() override {}
  void AcceptAppModalDialog() override {}
  void CancelAppModalDialog() override {}
  bool IsShowing() const override { return true; }

 private:
  std::unique_ptr<AppModalDialogController> controller_;
};

class TestDialogControllerBase : public AppModalDialogController {
 public:
  explicit TestDialogControllerBase(bool* was_destroyed)
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

// A dialog controller whose ShowModalDialog() sets `view_` to the fake owning
// view, allowing CompleteDialog() to trigger ShowNextDialog() on destruction.
class TestDialogController : public TestDialogControllerBase {
 public:
  explicit TestDialogController(bool* was_shown,
                                bool* was_destroyed,
                                std::unique_ptr<FakeView>* owning_view)
      : TestDialogControllerBase(was_destroyed),
        was_shown_(was_shown),
        owning_view_(owning_view) {}

  void ShowModalDialog(
      std::unique_ptr<AppModalDialogController> controller) override {
    if (was_shown_) {
      *was_shown_ = true;
    }
    *owning_view_ = std::make_unique<FakeView>(std::move(controller));
    view_ = owning_view_->get();
  }

  void TriggerComplete() { CompleteDialog(); }

 private:
  raw_ptr<bool> was_shown_ = nullptr;
  raw_ptr<std::unique_ptr<FakeView>> owning_view_ = nullptr;
};

// A dialog controller that fails the test if shown. Used to verify that
// CancelAllDialogs() prevents queued dialogs from being displayed.
class FailIfShownDialogController : public TestDialogControllerBase {
 public:
  using TestDialogControllerBase::TestDialogControllerBase;

  void ShowModalDialog(
      std::unique_ptr<AppModalDialogController> controller) override {
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
  std::unique_ptr<FakeView> dialog1_view;
  bool dialog2_destroyed = false;

  auto dialog1 = std::make_unique<TestDialogController>(
      &dialog1_shown, /*was_destroyed=*/nullptr, &dialog1_view);
  auto* dialog1_ptr = dialog1.get();

  queue->AddDialog(std::move(dialog1));
  ASSERT_TRUE(dialog1_shown);
  ASSERT_TRUE(queue->HasActiveDialog());

  auto dialog2 =
      std::make_unique<FailIfShownDialogController>(&dialog2_destroyed);

  queue->AddDialog(std::move(dialog2));
  ASSERT_FALSE(dialog2_destroyed);

  // Without this call, dialog1->TriggerComplete() would show dialog2.
  queue->CancelAllDialogs();
  EXPECT_TRUE(dialog2_destroyed);

  dialog1_ptr->TriggerComplete();
  EXPECT_FALSE(queue->HasActiveDialog());

  dialog1_ptr->Invalidate();
}

TEST_F(AppModalDialogQueueTest, AddDialogDeletesAfterShutdown) {
  auto* queue = AppModalDialogQueue::GetInstance();

  queue->CancelAllDialogs();

  bool destroyed = false;
  auto dialog = std::make_unique<FailIfShownDialogController>(&destroyed);
  queue->AddDialog(std::move(dialog));

  EXPECT_TRUE(destroyed);
  EXPECT_FALSE(queue->HasActiveDialog());
}

TEST_F(AppModalDialogQueueTest, ShowNextDialogDrainsQueueAfterShutdown) {
  auto* queue = AppModalDialogQueue::GetInstance();

  bool dialog1_shown = false;
  std::unique_ptr<FakeView> dialog1_view;

  auto dialog1 = std::make_unique<TestDialogController>(
      &dialog1_shown, /*was_destroyed=*/nullptr, &dialog1_view);
  auto* dialog1_ptr = dialog1.get();

  queue->AddDialog(std::move(dialog1));
  ASSERT_TRUE(dialog1_shown);

  bool destroyed2 = false;
  bool destroyed3 = false;
  queue->AddDialog(std::make_unique<FailIfShownDialogController>(&destroyed2));
  queue->AddDialog(std::make_unique<FailIfShownDialogController>(&destroyed3));

  queue->CancelAllDialogs();
  EXPECT_TRUE(destroyed2);
  EXPECT_TRUE(destroyed3);

  dialog1_ptr->TriggerComplete();
  EXPECT_FALSE(queue->HasActiveDialog());

  dialog1_ptr->Invalidate();
}

}  // namespace
}  // namespace javascript_dialogs
