// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/html_form_popup_observer.h"

#include <memory>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer_type.h"

namespace autofill {
namespace {

using ::aura::Window;
using ::aura::client::TransientWindowClient;
using ::aura::client::WINDOW_TYPE_CONTROL;
using ::aura::client::WINDOW_TYPE_MENU;
using ::aura::client::WINDOW_TYPE_NORMAL;
using ::aura::client::WINDOW_TYPE_POPUP;
using ::testing::Mock;
using ::ui::LAYER_NOT_DRAWN;

class HtmlFormPopupObserverTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    profile_ = std::make_unique<TestingProfile>();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    ASSERT_TRUE(web_contents_->GetNativeView());
    ASSERT_TRUE(aura::client::GetTransientWindowClient());
  }

  void TearDown() override {
    web_contents_.reset();
    profile_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void DrainTasks() {
    base::RunLoop run_loop;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  TransientWindowClient* transient_client() {
    return aura::client::GetTransientWindowClient();
  }

  content::WebContents* web_contents() { return web_contents_.get(); }
  void DestroyWebContents() { web_contents_.reset(); }

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// Tests that attaching a menu transient child inside the WebContents notifies
// asynchronously.
TEST_F(HtmlFormPopupObserverTest,
       NotifiesOnMenuTransientChildInsideWebContents) {
  base::MockRepeatingClosure callback;
  HtmlFormPopupObserver observer(*web_contents(), callback.Get());

  // 1. Direct attachment on the WebContents's native view.
  Window menu_popup(nullptr, WINDOW_TYPE_MENU);
  menu_popup.Init(LAYER_NOT_DRAWN);
  EXPECT_CALL(callback, Run()).Times(0);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu_popup);
  Mock::VerifyAndClearExpectations(&callback);

  EXPECT_CALL(callback, Run()).Times(1);
  DrainTasks();
  Mock::VerifyAndClearExpectations(&callback);
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu_popup);

  // 2. Attachment on a descendant window (e.g. a subframe
  // RenderWidgetHostView).
  Window child_view(nullptr, WINDOW_TYPE_CONTROL);
  child_view.Init(LAYER_NOT_DRAWN);
  web_contents()->GetNativeView()->AddChild(&child_view);

  Window subframe_menu(nullptr, WINDOW_TYPE_MENU);
  subframe_menu.Init(LAYER_NOT_DRAWN);
  EXPECT_CALL(callback, Run()).Times(1);
  transient_client()->AddTransientChild(&child_view, &subframe_menu);
  DrainTasks();
  transient_client()->RemoveTransientChild(&child_view, &subframe_menu);
  web_contents()->GetNativeView()->RemoveChild(&child_view);
}

// Tests that non-menu transient children inside the WebContents are ignored.
TEST_F(HtmlFormPopupObserverTest,
       IgnoresNonMenuTransientChildInsideWebContents) {
  base::MockRepeatingClosure callback;
  HtmlFormPopupObserver observer(*web_contents(), callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  Window non_menu_popup(nullptr, WINDOW_TYPE_POPUP);
  non_menu_popup.Init(LAYER_NOT_DRAWN);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &non_menu_popup);
  DrainTasks();
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &non_menu_popup);
}

// Tests that menu transient children outside the WebContents are ignored.
TEST_F(HtmlFormPopupObserverTest, IgnoresMenuTransientChildOutsideWebContents) {
  base::MockRepeatingClosure callback;
  HtmlFormPopupObserver observer(*web_contents(), callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  Window unrelated_parent(nullptr, WINDOW_TYPE_NORMAL);
  unrelated_parent.Init(LAYER_NOT_DRAWN);
  Window unrelated_menu(nullptr, WINDOW_TYPE_MENU);
  unrelated_menu.Init(LAYER_NOT_DRAWN);
  transient_client()->AddTransientChild(&unrelated_parent, &unrelated_menu);
  DrainTasks();
  transient_client()->RemoveTransientChild(&unrelated_parent, &unrelated_menu);
}

// Tests that burst additions coalesce into one callback and re-arm afterwards.
TEST_F(HtmlFormPopupObserverTest,
       CoalescesMultipleAdditionsAndNotifiesAgainLater) {
  base::MockRepeatingClosure callback;
  HtmlFormPopupObserver observer(*web_contents(), callback.Get());

  Window menu1(nullptr, WINDOW_TYPE_MENU);
  menu1.Init(LAYER_NOT_DRAWN);
  Window menu2(nullptr, WINDOW_TYPE_MENU);
  menu2.Init(LAYER_NOT_DRAWN);

  EXPECT_CALL(callback, Run()).Times(1);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu1);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu2);
  DrainTasks();
  Mock::VerifyAndClearExpectations(&callback);

  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu1);
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu2);

  // Subsequent additions after the first task ran should notify again.
  Window menu3(nullptr, WINDOW_TYPE_MENU);
  menu3.Init(LAYER_NOT_DRAWN);
  EXPECT_CALL(callback, Run()).Times(1);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu3);
  DrainTasks();
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu3);
}

// Tests that destroying the observer cancels a pending notification task.
TEST_F(HtmlFormPopupObserverTest, DestructionCancelsPendingNotification) {
  base::MockRepeatingClosure callback;
  auto observer =
      std::make_unique<HtmlFormPopupObserver>(*web_contents(), callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  Window menu_popup(nullptr, WINDOW_TYPE_MENU);
  menu_popup.Init(LAYER_NOT_DRAWN);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu_popup);
  observer.reset();
  DrainTasks();
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu_popup);
}

// Tests that destroying the observer synchronously inside its callback is safe.
TEST_F(HtmlFormPopupObserverTest, SafeToDestroyObserverInsideCallback) {
  int call_count = 0;
  std::unique_ptr<HtmlFormPopupObserver> observer;
  observer = std::make_unique<HtmlFormPopupObserver>(
      *web_contents(), base::BindLambdaForTesting([&] {
        ++call_count;
        observer.reset();
      }));

  Window menu1(nullptr, WINDOW_TYPE_MENU);
  menu1.Init(LAYER_NOT_DRAWN);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu1);
  DrainTasks();
  EXPECT_EQ(call_count, 1);
  EXPECT_FALSE(observer);
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu1);

  // Adding another transient child after observer destruction does nothing.
  Window menu2(nullptr, WINDOW_TYPE_MENU);
  menu2.Init(LAYER_NOT_DRAWN);
  transient_client()->AddTransientChild(web_contents()->GetNativeView(),
                                        &menu2);
  DrainTasks();
  EXPECT_EQ(call_count, 1);
  transient_client()->RemoveTransientChild(web_contents()->GetNativeView(),
                                           &menu2);
}

// Tests that transient events after WebContents teardown are ignored safely.
TEST_F(HtmlFormPopupObserverTest, IgnoresEventsAfterWebContentsDestroyed) {
  base::MockRepeatingClosure callback;
  HtmlFormPopupObserver observer(*web_contents(), callback.Get());
  EXPECT_CALL(callback, Run()).Times(0);

  DestroyWebContents();

  Window other_parent(nullptr, WINDOW_TYPE_NORMAL);
  other_parent.Init(LAYER_NOT_DRAWN);
  Window menu_popup(nullptr, WINDOW_TYPE_MENU);
  menu_popup.Init(LAYER_NOT_DRAWN);
  transient_client()->AddTransientChild(&other_parent, &menu_popup);
  DrainTasks();
  transient_client()->RemoveTransientChild(&other_parent, &menu_popup);
}

}  // namespace
}  // namespace autofill
