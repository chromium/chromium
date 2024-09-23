// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TEST_VIEW_EVENT_TEST_BASE_H_
#define CHROME_BROWSER_UI_VIEWS_TEST_VIEW_EVENT_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"

// We only want to use ViewEventTestBase in test targets which properly
// isolate each test case by running each test in a separate process.
// This way if a test hangs the test launcher can reliably terminate it.
#if !defined(HAS_OUT_OF_PROC_TEST_RUNNER)
#error Can't reliably terminate hanging event tests without OOP test runner.
#endif

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
namespace display {
class Screen;
}
#endif

namespace gfx {
class Size;
}

class TestBaseWidgetDelegate;

// Base class for Views based tests that dispatch events.
//
// As views based event test involves waiting for events to be processed,
// writing a views based test is slightly different than that of writing
// other unit tests. In particular when the test fails or is done you need
// to stop the message loop. This can be done by way of invoking the Done
// method.
//
// Any delayed callbacks should be done by way of CreateEventTask.
// CreateEventTask checks to see if ASSERT_XXX has been invoked after invoking
// the task. If there was a failure Done is invoked and the test stops.
//
// ViewEventTestBase creates a Window with the View returned from
// CreateContentsView. The preferred size for the view can be customized by
// overriding GetPreferredSizeForContents. If you do not override
// GetPreferredSizeForContents the preferred size of the view returned from
// CreateContentsView is used.
//
// Subclasses of ViewEventTestBase must implement two methods:
// . DoTestOnMessageLoop: invoked when the message loop is running. Run your
//   test here, invoke Done when done.
// . CreateContentsView: returns the view to place in the window.
//
// Once you have created a ViewEventTestBase use the macro VIEW_TEST to define
// the fixture.
//
// Testing drag and drop is tricky because the mouse move that initiates drag
// and drop may trigger a nested native event loop that waits for more mouse
// messages.  Once a drag begins, all UI events until the drag ends must be
// driven from observer callbacks and posted on the task runner returned by
// GetDragTaskRunner().

class ViewEventTestBase : public ChromeViewsTestBase {
 public:
  ViewEventTestBase();
  ViewEventTestBase(const ViewEventTestBase&) = delete;
  ViewEventTestBase& operator=(const ViewEventTestBase&) = delete;
  ~ViewEventTestBase() override;

  static void SetUpTestSuite();

  // ChromeViewsTestBase:
  void SetUp() override;
  void TearDown() override;
  views::Widget::InitParams CreateParams(
      views::Widget::InitParams::Ownership ownership,
      views::Widget::InitParams::Type type) override;

  // Returns the view that is added to the window.
  virtual std::unique_ptr<views::View> CreateContentsView() = 0;

  // Returns an empty Size. Subclasses that want a preferred size other than
  // that of the View returned by CreateContentsView should override this
  // appropriately.
  virtual gfx::Size GetPreferredSizeForContents() const;

  // Invoke when done either because of failure or success. Quits the message
  // loop.
  void Done();

  views::Widget* window() { return window_; }

 protected:
  // Called once the message loop is running.
  virtual void DoTestOnMessageLoop() = 0;

  // Invoke from test main. Shows the window, starts the message loop and
  // schedules a task that invokes DoTestOnMessageLoop.
  void StartMessageLoopAndRunTest();

  // Creates a task that calls the specified method back. The specified
  // method is called in such a way that if there are any test failures
  // Done is invoked.
  template <class T, class Method>
  base::OnceClosure CreateEventTask(T* target, Method method) {
    return base::BindOnce(&ViewEventTestBase::RunTestMethod,
                          base::Unretained(this),
                          base::BindOnce(method, base::Unretained(target)));
  }

  // Callback from CreateEventTask. Runs the supplied task and if there are
  // failures invokes Done.
  void RunTestMethod(base::OnceClosure task);

  // Returns a task runner to use for drag-related mouse events.
  scoped_refptr<base::SingleThreadTaskRunner> GetDragTaskRunner();

 private:
  friend class TestBaseWidgetDelegate;

  ui::AXPlatformForTest ax_platform_;

#if defined(USE_AURA) && !BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<display::Screen> screen_;
#endif

  // Thread for posting background drag events.
  std::unique_ptr<base::Thread> drag_event_thread_;

  base::RunLoop run_loop_;
  raw_ptr<views::Widget> window_ = nullptr;
};

// Convenience macro for defining a ViewEventTestBase. See class description
// of ViewEventTestBase for details.
#define VIEW_TEST(test_class, name) \
  TEST_F(test_class, name) { StartMessageLoopAndRunTest(); }

#endif  // CHROME_BROWSER_UI_VIEWS_TEST_VIEW_EVENT_TEST_BASE_H_
