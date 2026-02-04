// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"
#include "base/scoped_observation.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace {
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWindowHidden);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWindowMinimized);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWindowShown);
DEFINE_LOCAL_CUSTOM_ELEMENT_EVENT_TYPE(kWindowRestored);
}  // namespace

// This test class tests assumptions about the order in which events happen when
// a window is minimized and restored, so that they can be handled properly by
// the `ElementTracker` implementation.
//
// A window that is minimized should not be counted as not-visible, even if the
// system believes the window is not visible. These are regression tests to
// ensure that the platform-specific implementations continue to conform to the
// set of cases we are capable of handling.
//
// Notes:
//  - Not all platforms change visibility on minimize - Windows, for example,
//    does not.
//  - The code path for updating minimized state and/or visibility is different
//    on each platform.
//  - In some cases the order depends on the order of observers.
//
class ElementTrackerViewsMinimizeRestoreUiTest : public InteractiveBrowserTest,
                                                 public views::WidgetObserver {
 public:
  ElementTrackerViewsMinimizeRestoreUiTest() = default;
  ~ElementTrackerViewsMinimizeRestoreUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    widget_observation_.Observe(context_widget());
  }

  void TearDownOnMainThread() override {
    widget_observation_.Reset();
    InteractiveBrowserTest::TearDownOnMainThread();
  }

 protected:
  views::Widget* GetWidget() { return context_widget(); }

  ui::TrackedElement* GetTargetElement() {
    return private_test_impl().GetPivotElement(
        private_test_impl().default_context());
  }

  void SendEvent(ui::CustomElementEventType event) {
    ui::ElementTracker::GetFrameworkDelegate()->NotifyCustomEvent(
        GetTargetElement(), event);
  }

 private:
  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget*, bool visible) override {
    LOG(INFO) << "------------- Window visibility set to " << visible << "\n"
              << base::debug::StackTrace(20);
    if (visible) {
      SendEvent(kWindowShown);
    } else {
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindLambdaForTesting([this]() {
            LOG(INFO) << "------------- Window hidden";
            SendEvent(kWindowHidden);
          }));
    }
  }
  void OnWidgetShowStateChanged(views::Widget* widget) override {
    LOG(INFO) << "------------- Window state set to "
              << (widget->IsMinimized() ? "minimized" : "restored") << "\n"
              << base::debug::StackTrace(20);
    ;
    SendEvent(widget->IsMinimized() ? kWindowMinimized : kWindowRestored);
  }

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

IN_PROC_BROWSER_TEST_F(ElementTrackerViewsMinimizeRestoreUiTest,
                       TestAssumptions) {
  // Note: on Windows, visibility does not change on minimization.
#if !BUILDFLAG(IS_WIN)
  bool shown = false;
#endif

  RunTestSequence(
      Check([this]() { return GetWidget()->IsVisible(); }),
      Do([this]() { GetWidget()->Minimize(); }),
      WithoutDelay(
          AfterEvent(ui::test::internal::kInteractiveTestPivotElementId,
                     kWindowMinimized, []() { LOG(INFO) << "GOT MINIMIZED"; })
#if !BUILDFLAG(IS_WIN)
              ,
          AfterEvent(ui::test::internal::kInteractiveTestPivotElementId,
                     kWindowHidden, []() { LOG(INFO) << "GOT HIDDEN"; })
#endif
              ),
      Do([this]() { GetWidget()->Restore(); }),
#if BUILDFLAG(IS_WIN)
      AfterEvent(ui::test::internal::kInteractiveTestPivotElementId,
                 kWindowRestored, []() { LOG(INFO) << "GOT RESTORED"; })
#else
      InParallel(
          RunSubsequence(WithoutDelay(AfterEvent(
              ui::test::internal::kInteractiveTestPivotElementId, kWindowShown,
              [&]() {
                LOG(INFO) << "GOT SHOWN";
                shown = true;
              }))),
          RunSubsequence(WithoutDelay(
              AfterEvent(ui::test::internal::kInteractiveTestPivotElementId,
                         kWindowRestored,
                         []() { LOG(INFO) << "GOT RESTORED"; }),
              CheckVariable(shown, true))))
#endif

  );
}
