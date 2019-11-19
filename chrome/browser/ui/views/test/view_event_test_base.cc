// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/test/view_event_test_base.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ui/views/test/view_event_test_platform_part.h"
#include "chrome/test/base/chrome_unit_test_suite.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "mojo/core/embedder/embedder.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/ime/init/input_method_initializer.h"
#include "ui/compositor/test/test_context_factories.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// View subclass that allows you to specify the preferred size.
class TestView : public views::View {
 public:
  explicit TestView(ViewEventTestBase* harness) : harness_(harness) {}

  gfx::Size CalculatePreferredSize() const override {
    return harness_->GetPreferredSizeForContents();
  }

  void Layout() override {
    // Permit a test to remove the view being tested from the hierarchy, then
    // still handle a _NET_WM_STATE event on Linux during teardown that triggers
    // layout.
    if (!children().empty())
      children().front()->SetBoundsRect(GetLocalBounds());
  }

 private:
  ViewEventTestBase* harness_;

  DISALLOW_COPY_AND_ASSIGN(TestView);
};

}  // namespace

ViewEventTestBase::ViewEventTestBase() {
  // The TestingBrowserProcess must be created in the constructor because there
  // are tests that require it before SetUp() is called.
  TestingBrowserProcess::CreateInstance();

  // Mojo is initialized here similar to how each browser test case initializes
  // Mojo when starting. This only works because each interactive_ui_test runs
  // in a new process.
  mojo::core::Init();
}

void ViewEventTestBase::Done() {
  drag_event_thread_.reset();
  run_loop_.Quit();
}

void ViewEventTestBase::SetUpTestCase() {
  ChromeUnitTestSuite::InitializeProviders();
  ChromeUnitTestSuite::InitializeResourceBundle();
}

void ViewEventTestBase::SetUp() {
  ui::InitializeInputMethodForTesting();

  // The ContextFactory must exist before any Compositors are created.
  const bool enable_pixel_output = false;
  context_factories_ =
      std::make_unique<ui::TestContextFactories>(enable_pixel_output);

  views_delegate_.set_context_factory(context_factories_->GetContextFactory());
  views_delegate_.set_context_factory_private(
      context_factories_->GetContextFactoryPrivate());
  views_delegate_.set_use_desktop_native_widgets(true);

  platform_part_.reset(ViewEventTestPlatformPart::Create(
      context_factories_->GetContextFactory(),
      context_factories_->GetContextFactoryPrivate()));
  gfx::NativeWindow context = platform_part_->GetContext();
  window_ = views::Widget::CreateWindowWithContext(this, context);
  window_->Show();
}

void ViewEventTestBase::TearDown() {
  if (window_) {
    window_->Close();
    base::RunLoop().RunUntilIdle();
    window_ = NULL;
  }

  ui::Clipboard::DestroyClipboardForCurrentThread();
  platform_part_.reset();

  context_factories_.reset();

  ui::ShutdownInputMethodForTesting();
}

gfx::Size ViewEventTestBase::GetPreferredSizeForContents() const {
  return gfx::Size();
}

bool ViewEventTestBase::CanResize() const {
  return true;
}

views::View* ViewEventTestBase::GetContentsView() {
  if (!content_view_) {
    // Wrap the real view (as returned by CreateContentsView) in a View so
    // that we can customize the preferred size.
    TestView* test_view = new TestView(this);
    test_view->AddChildView(CreateContentsView());
    content_view_ = test_view;
  }
  return content_view_;
}

const views::Widget* ViewEventTestBase::GetWidget() const {
  return content_view_->GetWidget();
}

views::Widget* ViewEventTestBase::GetWidget() {
  return content_view_->GetWidget();
}

ViewEventTestBase::~ViewEventTestBase() {
  TestingBrowserProcess::DeleteInstance();
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
