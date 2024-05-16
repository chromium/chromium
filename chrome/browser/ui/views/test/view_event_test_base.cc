// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/test/view_event_test_base.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/display/screen.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/views/widget/desktop_aura/desktop_screen.h"

#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    BUILDFLAG(IS_OZONE)
#include "ui/views/test/test_desktop_screen_ozone.h"
#endif  // (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) &&
        // BUILDFLAG(IS_OZONE)
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

  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override {
    return harness_->GetPreferredSizeForContents();
  }

 private:
  raw_ptr<ViewEventTestBase> harness_;
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
  views::Widget* GetWidget() override {
    return contents_ ? contents_->GetWidget() : nullptr;
  }
  const views::Widget* GetWidget() const override {
    return contents_ ? contents_->GetWidget() : nullptr;
  }
  views::View* GetContentsView() override {
    // This will first be called by Widget::Init(), which passes the returned
    // View* to SetContentsView(), which takes ownership.
    if (!contents_) {
      contents_ = new TestView(harness_);
    }
    return contents_;
  }

 private:
  raw_ptr<ViewEventTestBase> harness_;
  raw_ptr<views::View> contents_ = nullptr;
};

ViewEventTestBase::ViewEventTestBase() {
  // The TestingBrowserProcess must be created in the constructor because there
  // are tests that require it before SetUp() is called.
  TestingBrowserProcess::CreateInstance();

  // Mojo is initialized here similar to how each browser test case initializes
  // Mojo when starting. This only works because each interactive_ui_test runs
  // in a new process.
  mojo::core::Init();

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(pkasting): Determine why the TestScreen in AuraTestHelper is
  // insufficient for these tests, then either bolster/replace it or fix the
  // tests.
  DCHECK(!display::Screen::HasScreen());
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)) && \
    BUILDFLAG(IS_OZONE)
  if (!display::Screen::HasScreen()) {
    screen_ = views::test::TestDesktopScreenOzone::Create();
  }
#endif
  if (!display::Screen::HasScreen()) {
    screen_ = views::CreateDesktopScreen();
  }
#endif
}

ViewEventTestBase::~ViewEventTestBase() {
  TestingBrowserProcess::DeleteInstance();
}

void ViewEventTestBase::SetUpTestSuite() {
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();
}

void ViewEventTestBase::SetUp() {
  ChromeViewsTestBase::SetUp();

  test_views_delegate()->set_use_desktop_native_widgets(true);

  window_ = AllocateTestWidget().release();
  window_->Init(
      CreateParams(views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
                   views::Widget::InitParams::TYPE_WINDOW));
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
    views::Widget::InitParams::Ownership ownership,
    views::Widget::InitParams::Type type) {
  views::Widget::InitParams params =
      ChromeViewsTestBase::CreateParams(ownership, type);
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
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ViewEventTestBase::DoTestOnMessageLoop,
                                base::Unretained(this)));

  run_loop_.Run();
}

void ViewEventTestBase::RunTestMethod(base::OnceClosure task) {
  std::move(task).Run();
  if (HasFatalFailure()) {
    Done();
  }
}

scoped_refptr<base::SingleThreadTaskRunner>
ViewEventTestBase::GetDragTaskRunner() {
#if BUILDFLAG(IS_WIN)
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
  return base::SingleThreadTaskRunner::GetCurrentDefault();
#endif
}
