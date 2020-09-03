// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/test/view_event_test_base.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
#include "ui/display/screen.h"
#include "ui/views/widget/desktop_aura/desktop_screen.h"

#if defined(USE_X11)
#include "ui/views/test/test_desktop_screen_x11.h"
#endif  // defined(USE_X11)
#endif

namespace {

// View that keeps its preferred size in sync with what |harness| requests.
class TestView : public views::View {
 public:
  explicit TestView(ViewEventTestBase* harness) : harness_(harness) {
    SetLayoutManager(std::make_unique<views::FillLayout>());
    AddChildView(harness_->CreateContentsView());
  }
  TestView(const TestView&) = delete;
  TestView& operator=(const TestView&) = delete;
  ~TestView() override = default;

  gfx::Size CalculatePreferredSize() const override {
    return harness_->GetPreferredSizeForContents();
  }

 private:
  ViewEventTestBase* harness_;
};

}  // namespace

class TestBaseWidgetDelegate : public views::WidgetDelegate {
 public:
  explicit TestBaseWidgetDelegate(ViewEventTestBase* harness)
      : harness_(harness) {
    SetCanResize(true);
    SetOwnedByWidget(true);
  }
  TestBaseWidgetDelegate(const TestBaseWidgetDelegate&) = delete;
  TestBaseWidgetDelegate& operator=(const TestBaseWidgetDelegate&) = delete;
  ~TestBaseWidgetDelegate() override = default;

  // views::WidgetDelegate:
  void WindowClosing() override { harness_->window_ = nullptr; }
  views::Widget* GetWidget() override { return contents_->GetWidget(); }
  const views::Widget* GetWidget() const override {
    return contents_->GetWidget();
  }
  views::View* GetContentsView() override {
    // This will first be called by Widget::Init(), which passes the returned
    // View* to SetContentsView(), which takes ownership.
    if (!contents_)
      contents_ = new TestView(harness_);
    return contents_;
  }

 private:
  ViewEventTestBase* harness_;
  views::View* contents_ = nullptr;
};

ViewEventTestBase::ViewEventTestBase() {
  // The TestingBrowserProcess must be created in the constructor because there
  // are tests that require it before SetUp() is called.
  TestingBrowserProcess::CreateInstance();

  // Mojo is initialized here similar to how each browser test case initializes
  // Mojo when starting. This only works because each interactive_ui_test runs
  // in a new process.
  mojo::core::Init();

#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  // TODO(pkasting): Determine why the TestScreen in AuraTestHelper is
  // insufficient for these tests, then either bolster/replace it or fix the
  // tests.
  DCHECK(!display::Screen::GetScreen());
#if defined(USE_X11)
  if (!features::IsUsingOzonePlatform())
    views::test::TestDesktopScreenX11::GetInstance();
#endif  // defined(USE_X11)
  if (!display::Screen::GetScreen())
    screen_.reset(views::CreateDesktopScreen());
#endif
}

ViewEventTestBase::~ViewEventTestBase() {
  TestingBrowserProcess::DeleteInstance();
}

void ViewEventTestBase::SetUpTestCase() {
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();
}

void ViewEventTestBase::SetUp() {
  ChromeViewsTestBase::SetUp();

  test_views_delegate()->set_use_desktop_native_widgets(true);

  window_ = AllocateTestWidget().release();
  window_->Init(CreateParams(views::Widget::InitParams::TYPE_WINDOW));
  window_->Show();
}

void ViewEventTestBase::TearDown() {
  if (window_) {
    window_->Close();
    base::RunLoop().RunUntilIdle();
  }

  ChromeViewsTestBase::TearDown();
}

views::Widget::InitParams ViewEventTestBase::CreateParams(
    views::Widget::InitParams::Type type) {
  views::Widget::InitParams params = ChromeViewsTestBase::CreateParams(type);
  params.delegate = new TestBaseWidgetDelegate(this);  // Owns itself.
  return params;
}

gfx::Size ViewEventTestBase::GetPreferredSizeForContents() const {
  return gfx::Size();
}

void ViewEventTestBase::Done() {
  drag_event_thread_.reset();
  run_loop_.Quit();
}

void ViewEventTestBase::StartMessageLoopAndRunTest() {
  ASSERT_TRUE(
      ui_test_utils::ShowAndFocusNativeWindow(window_->GetNativeWindow()));

  // Flush any pending events to make sure we start with a clean slate.
  base::RunLoop().RunUntilIdle();

  // Schedule a task that starts the test. Need to do this as we're going to
  // run the message loop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&ViewEventTestBase::DoTestOnMessageLoop,
                                base::Unretained(this)));

  run_loop_.Run();
}

scoped_refptr<base::SingleThreadTaskRunner>
ViewEventTestBase::GetDragTaskRunner() {
#if defined(OS_WIN)
  // Drag events must be posted from a background thread, since starting a drag
  // triggers a nested message loop that filters messages other than mouse
  // events, so further tasks on the main message loop will be blocked.
  if (!drag_event_thread_) {
    drag_event_thread_ = std::make_unique<base::Thread>("drag-event-thread");
    drag_event_thread_->Start();
  }
  return drag_event_thread_->task_runner();
#else
  // Drag events must be posted from the current thread, since UI events on many
  // platforms cannot be posted from background threads.  The nested drag
  // message loop on non-Windows does not filter out non-input events, so these
  // tasks will run.
  return base::ThreadTaskRunnerHandle::Get();
#endif
}

void ViewEventTestBase::RunTestMethod(base::OnceClosure task) {
  std::move(task).Run();
  if (HasFatalFailure())
    Done();
}
