// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/apc_scrim_manager_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/timer/mock_timer.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/render_widget_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

using ::testing::StrictMock;

class TestApcScrimManagerImpl : public ApcScrimManagerImpl {
 public:
  explicit TestApcScrimManagerImpl(content::WebContents* web_contents)
      : ApcScrimManagerImpl(web_contents) {}

  void OnVisibilityChanged(content::Visibility visibility) override {
    ApcScrimManagerImpl::OnVisibilityChanged(visibility);
  }

  bool WasFocusOnWebContentsCalled() {
    return was_focus_on_web_contents_called_;
  }

  void FocusOnWebContents() override {
    was_focus_on_web_contents_called_ = true;
  }

 private:
  bool was_focus_on_web_contents_called_ = false;
};

class ApcScrimManagerImplTest : public TestWithBrowserView {
 public:
  ApcScrimManagerImplTest() : ax_mode_setter_(ui::kAXModeComplete) {}
  ~ApcScrimManagerImplTest() override = default;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);
    content::WebContents* active_contents =
        browser_view()->GetActiveWebContents();

    apc_scrim_manager_ =
        std::make_unique<TestApcScrimManagerImpl>(active_contents);

    web_contents_ = active_contents;
  }

  void TearDown() override {
    if (apc_scrim_manager_)
      apc_scrim_manager_.reset();
    TestWithBrowserView::TearDown();
  }

  TestApcScrimManagerImpl* apc_scrim_manager() {
    return apc_scrim_manager_.get();
  }

  content::WebContents* web_contents() { return web_contents_; }

 private:
  content::testing::ScopedContentAXModeSetter ax_mode_setter_;
  std::unique_ptr<TestApcScrimManagerImpl> apc_scrim_manager_;
  content::WebContents* web_contents_;
};

TEST_F(ApcScrimManagerImplTest,
       UpdatesVisibilityOnWebcontentsVisibilityChanged) {
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->Show();
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::HIDDEN);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::VISIBLE);
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());
}

TEST_F(ApcScrimManagerImplTest, HideDoesNotImmediatelySetOriginalAxMode) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  apc_scrim_manager()->SetRestoreAccessibilityModeTimerForTest(
      std::move(mock_timer));

  apc_scrim_manager()->Show();
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->Hide();
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
  ASSERT_EQ(web_contents()->GetAccessibilityMode(), ui::AXMode::kNone);
  timer_ptr->Fire();
  ASSERT_EQ(web_contents()->GetAccessibilityMode(), ui::kAXModeComplete);
}

TEST_F(ApcScrimManagerImplTest, HideFocusOnTheWebContent) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  apc_scrim_manager()->SetRestoreAccessibilityModeTimerForTest(
      std::move(mock_timer));

  apc_scrim_manager()->Show();
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->Hide();
  timer_ptr->Fire();
  ASSERT_TRUE(apc_scrim_manager()->WasFocusOnWebContentsCalled());
}

TEST_F(ApcScrimManagerImplTest, DisabledScrimCannotBeShown) {
  apc_scrim_manager()->SetIsDisabled(true);
  apc_scrim_manager()->Show();
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->SetIsDisabled(false);
  apc_scrim_manager()->Show();
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());
}

TEST_F(ApcScrimManagerImplTest, ShutDownDisablesAndHidesScrim) {
  apc_scrim_manager()->Show();
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());

  // Shutting down hides and disabled it.
  apc_scrim_manager()->ShutDown();
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
  ASSERT_EQ(web_contents()->GetAccessibilityMode(), ui::kAXModeComplete);
  ASSERT_TRUE(apc_scrim_manager()->GetIsDisabled());

  // When shutdown, focus is not on the webcontents.
  ASSERT_FALSE(apc_scrim_manager()->WasFocusOnWebContentsCalled());
}

TEST_F(ApcScrimManagerImplTest,
       RestoresTheOriginalHiddenStateOnWebcontentsVisibibleTransition) {
  auto mock_timer = std::make_unique<base::MockOneShotTimer>();
  base::MockOneShotTimer* timer_ptr = mock_timer.get();
  apc_scrim_manager()->SetRestoreAccessibilityModeTimerForTest(
      std::move(mock_timer));

  // If the webcontent is hidden right after construction,
  // goes back to a invisible scrim when the webcontent is visible.
  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::HIDDEN);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::VISIBLE);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->Show();
  ASSERT_TRUE(apc_scrim_manager()->GetVisible());

  // Make sure that a hidden scrim remains hidden
  // regardless the webcontent visibility change.
  apc_scrim_manager()->Hide();
  timer_ptr->Fire();
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());

  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::HIDDEN);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::VISIBLE);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
}
