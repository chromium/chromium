// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/javascript_dialogs/app_modal_dialog_queue.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
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
  // Real views notify their controller that the dialog was dismissed when they
  // are closed. Note that, unlike the real views, this one keeps `controller_`
  // alive so that tests control its lifetime.
  void CloseAppModalDialog() override {
    if (controller_) {
      controller_->OnCancel(/*suppress_js_messages=*/false);
    }
  }
  void AcceptAppModalDialog() override {}
  void CancelAppModalDialog() override {}
  bool IsShowing() const override { return true; }

 private:
  std::unique_ptr<AppModalDialogController> controller_;
};

class TestDialogControllerBase : public AppModalDialogController {
 public:
  explicit TestDialogControllerBase(
      bool* was_destroyed,
      content::JavaScriptDialogManager::DialogClosedCallback callback =
          base::DoNothing())
      : AppModalDialogController(nullptr,
                                 &extra_data_,
                                 u"test",
                                 content::JAVASCRIPT_DIALOG_TYPE_ALERT,
                                 u"msg",
                                 u"",
                                 false,
                                 false,
                                 false,
                                 std::move(callback)),
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

// Returns a DialogClosedCallback that re-entrantly cancels all dialogs for the
// test WebContents (nullptr), and records that it ran.
content::JavaScriptDialogManager::DialogClosedCallback
MakeReentrantCancelCallback(bool* callback_ran) {
  return base::BindOnce(
      [](bool* callback_ran, bool /*success*/,
         const std::u16string& /*user_input*/) {
        *callback_ran = true;
        // This invalidates the active dialog, whose view then dispatches
        // OnCancel -> CompleteDialog -> ShowNextDialog -> GetNextDialog, which
        // drains and destroys the invalidated dialogs still in the queue - one
        // of which is the dialog whose Invalidate() is running this callback.
        AppModalDialogManager::GetInstance()->CancelDialogs(
            /*web_contents=*/nullptr, /*reset_state=*/false);
      },
      callback_ran);
}

// Regression test for https://crbug.com/560439699: a queued dialog's closed
// callback re-entrantly cancels dialogs, which destroys the queued dialogs
// while AppModalDialogController::Invalidate() and the CancelDialogs() loop
// iterating the queue are still on the stack.
TEST_F(AppModalDialogQueueTest, ReentrantCancelDialogsDuringInvalidate) {
  auto* queue = AppModalDialogQueue::GetInstance();
  auto* manager = AppModalDialogManager::GetInstance();

  bool dialog1_shown = false;
  bool dialog1_destroyed = false;
  bool dialog2_destroyed = false;
  bool dialog3_destroyed = false;
  bool callback_ran = false;
  std::unique_ptr<FakeView> dialog1_view;

  queue->AddDialog(std::make_unique<TestDialogController>(
      &dialog1_shown, &dialog1_destroyed, &dialog1_view));
  ASSERT_TRUE(dialog1_shown);
  ASSERT_TRUE(queue->HasActiveDialog());

  queue->AddDialog(std::make_unique<TestDialogControllerBase>(
      &dialog2_destroyed, MakeReentrantCancelCallback(&callback_ran)));
  queue->AddDialog(
      std::make_unique<TestDialogControllerBase>(&dialog3_destroyed));

  manager->CancelDialogs(/*web_contents=*/nullptr, /*reset_state=*/false);

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(dialog2_destroyed);
  EXPECT_TRUE(dialog3_destroyed);
  EXPECT_FALSE(queue->HasActiveDialog());
  // The active dialog is owned by its view, so it outlives the cancellation.
  EXPECT_FALSE(dialog1_destroyed);
}

// Regression test for https://crbug.com/560439699: the same re-entrancy, but
// entered through AppModalDialogQueue::InvalidateAndClearQueuedDialogs(). The
// re-entrant ShowNextDialog() must not drain (and destroy) the dialogs that the
// outer invalidation loop is still walking over.
TEST_F(AppModalDialogQueueTest, ReentrantCancelDuringCancelAllDialogs) {
  auto* queue = AppModalDialogQueue::GetInstance();

  bool dialog1_shown = false;
  bool dialog1_destroyed = false;
  bool dialog2_destroyed = false;
  bool dialog3_destroyed = false;
  bool callback_ran = false;
  std::unique_ptr<FakeView> dialog1_view;

  queue->AddDialog(std::make_unique<TestDialogController>(
      &dialog1_shown, &dialog1_destroyed, &dialog1_view));
  ASSERT_TRUE(dialog1_shown);
  ASSERT_TRUE(queue->HasActiveDialog());

  queue->AddDialog(std::make_unique<TestDialogControllerBase>(
      &dialog2_destroyed, MakeReentrantCancelCallback(&callback_ran)));
  queue->AddDialog(
      std::make_unique<TestDialogControllerBase>(&dialog3_destroyed));

  queue->CancelAllDialogs();

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(dialog2_destroyed);
  EXPECT_TRUE(dialog3_destroyed);
  EXPECT_FALSE(queue->HasActiveDialog());
  // The active dialog is owned by its view, so it outlives the cancellation.
  EXPECT_FALSE(dialog1_destroyed);
}

}  // namespace
}  // namespace javascript_dialogs
