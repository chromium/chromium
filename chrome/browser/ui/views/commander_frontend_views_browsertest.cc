// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commander_frontend_views.h"

#include <tuple>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/commander/commander_backend.h"
#include "chrome/browser/ui/commander/commander_view_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"

class CommanderFrontendViewsTest : public InProcessBrowserTest {
 public:
  // TODO(lgrey): This is copied over from CommanderControllerUnittest, with
  // modifications. If we need it one more time, extract to a common file.
  class TestBackend : public commander::CommanderBackend {
   public:
    void OnTextChanged(const std::u16string& text, Browser* browser) override {
      text_changed_invocations_.push_back(text);
    }
    void OnCommandSelected(size_t command_index, int result_set_id) override {
      command_selected_invocations_.push_back(command_index);
    }
    void OnCompositeCommandCancelled() override {
      composite_command_cancelled_invocation_count_++;
    }
    void SetUpdateCallback(commander::CommanderBackend::ViewModelUpdateCallback
                               callback) override {
      callback_ = std::move(callback);
    }
    void Reset() override { reset_invocation_count_++; }

    void CallCallback() {
      commander::CommanderViewModel vm;
      CallCallback(vm);
    }

    void CallCallback(commander::CommanderViewModel vm) { callback_.Run(vm); }

    const std::vector<std::u16string> text_changed_invocations() {
      return text_changed_invocations_;
    }
    const std::vector<size_t> command_selected_invocations() {
      return command_selected_invocations_;
    }

    int composite_command_cancelled_invocation_count() {
      return composite_command_cancelled_invocation_count_;
    }
    int reset_invocation_count() { return reset_invocation_count_; }

   private:
    commander::CommanderBackend::ViewModelUpdateCallback callback_;
    std::vector<std::u16string> text_changed_invocations_;
    std::vector<size_t> command_selected_invocations_;
    int composite_command_cancelled_invocation_count_ = 0;
    int reset_invocation_count_ = 0;
  };

 protected:
  views::Widget* WaitForCommanderWidgetAttachedTo(Browser* browser) {
    if (IsWidgetAttachedToBrowser(browser)) {
      expected_browser_ = nullptr;
      return active_widget_;
    }
    expected_browser_ = browser;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    EXPECT_TRUE(IsWidgetAttachedToBrowser(browser));
    return active_widget_;
  }
  void WaitForCommanderWidgetToClose() {
    if (!active_widget_)
      return;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    EXPECT_TRUE(!active_widget_);
  }
  std::unique_ptr<TestBackend> backend_;

 private:
  void SetUpOnMainThread() override {
    backend_ = std::make_unique<TestBackend>();
    observer_ = std::make_unique<views::AnyWidgetObserver>(
        views::test::AnyWidgetTestPasskey());
    // Unretained is safe since we own observer.
    observer_->set_shown_callback(base::BindRepeating(
        &CommanderFrontendViewsTest::OnWidgetShown, base::Unretained(this)));
    observer_->set_closing_callback(base::BindRepeating(
        &CommanderFrontendViewsTest::OnWidgetClosed, base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    observer_.reset();
    run_loop_.reset();
    backend_.reset();
  }

  void OnWidgetShown(views::Widget* widget) {
    if (widget->GetName() == "Quick Commands") {
      active_widget_ = widget;
      if (IsWidgetAttachedToBrowser(expected_browser_)) {
        expected_browser_ = nullptr;
        if (run_loop_)
          run_loop_->Quit();
      }
    }
  }

  void OnWidgetClosed(views::Widget* widget) {
    if (widget == active_widget_) {
      active_widget_ = nullptr;
      if (run_loop_)
        run_loop_->Quit();
    }
  }

  bool IsWidgetAttachedToBrowser(const Browser* browser) {
    if (!active_widget_ || !browser)
      return false;
    views::Widget* browser_widget =
        BrowserView::GetBrowserViewForBrowser(browser)->GetWidget();
    return active_widget_->parent() == browser_widget;
  }

  std::unique_ptr<views::AnyWidgetObserver> observer_;
  raw_ptr<views::Widget> active_widget_ = nullptr;
  raw_ptr<Browser> expected_browser_ = nullptr;
  std::unique_ptr<base::RunLoop> run_loop_;
};

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ShowShowsWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));
  frontend->Hide();
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, HideHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  frontend->Hide();
  WaitForCommanderWidgetToClose();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, DismissHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  frontend->OnDismiss();
  WaitForCommanderWidgetToClose();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ViewModelCloseHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  commander::CommanderViewModel vm;
  vm.action = commander::CommanderViewModel::Action::kClose;
  backend_->CallCallback(vm);
  WaitForCommanderWidgetToClose();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ToggleShowsWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->ToggleForBrowser(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));

  frontend->Hide();
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ToggleHidesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));

  frontend->ToggleForBrowser(browser());
  WaitForCommanderWidgetToClose();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

// Ensure that calling toggle twice in a row does the right thing.
IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, ToggleTogglesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->ToggleForBrowser(browser());
  EXPECT_TRUE(WaitForCommanderWidgetAttachedTo(browser()));

  frontend->ToggleForBrowser(browser());
  WaitForCommanderWidgetToClose();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, OnHeightChangedSizesWidget) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  views::Widget* commander_widget = WaitForCommanderWidgetAttachedTo(browser());

  int old_height = commander_widget->GetRootView()->height();
  int new_height = 200;
  // Ensure changing height isn't a no-op.
  EXPECT_NE(old_height, new_height);

  frontend->OnHeightChanged(200);
  EXPECT_EQ(new_height, commander_widget->GetRootView()->height());
  frontend->Hide();
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, PassesOnOptionSelected) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  std::ignore = WaitForCommanderWidgetAttachedTo(browser());

  frontend->OnOptionSelected(8, 13);
  ASSERT_EQ(backend_->command_selected_invocations().size(), 1u);
  EXPECT_EQ(backend_->command_selected_invocations().back(), 8u);
  frontend->Hide();
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, PassesOnTextChanged) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());
  frontend->Show(browser());
  std::ignore = WaitForCommanderWidgetAttachedTo(browser());

  const std::u16string input = u"orange";
  frontend->OnTextChanged(input);
  ASSERT_EQ(backend_->text_changed_invocations().size(), 1u);
  EXPECT_EQ(backend_->text_changed_invocations().back(), input);
  frontend->Hide();
}

IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest,
                       PassesOnCompositeCommandCancelled) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());
  frontend->Show(browser());
  std::ignore = WaitForCommanderWidgetAttachedTo(browser());

  EXPECT_EQ(backend_->composite_command_cancelled_invocation_count(), 0);
  frontend->OnCompositeCommandCancelled();
  EXPECT_EQ(backend_->composite_command_cancelled_invocation_count(), 1);
  frontend->Hide();
}
IN_PROC_BROWSER_TEST_F(CommanderFrontendViewsTest, HidesOnFocusLoss) {
  auto frontend = std::make_unique<CommanderFrontendViews>(backend_.get());

  frontend->Show(browser());
  views::Widget* widget = WaitForCommanderWidgetAttachedTo(browser());
  EXPECT_TRUE(widget);
  EXPECT_EQ(backend_->reset_invocation_count(), 0);

  // Activate the main browser window.
  widget->parent()->Activate();
  WaitForCommanderWidgetToClose();
  EXPECT_EQ(backend_->reset_invocation_count(), 1);
}
