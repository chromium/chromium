// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/apc_scrim_manager_impl.h"

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;

class TestApcScrimManagerImpl : public ApcScrimManagerImpl {
 public:
  explicit TestApcScrimManagerImpl(content::WebContents* web_contents)
      : ApcScrimManagerImpl(web_contents) {}

  void OnVisibilityChanged(content::Visibility visibility) override {
    ApcScrimManagerImpl::OnVisibilityChanged(visibility);
  }
};

class ApcScrimManagerImplTest : public TestWithBrowserView {
 public:
  ApcScrimManagerImplTest() = default;
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

TEST_F(ApcScrimManagerImplTest,
       RestoresTheOriginalHiddenStateOnWebcontentsVisibibleTransition) {
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
  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::HIDDEN);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
  apc_scrim_manager()->OnVisibilityChanged(content::Visibility::VISIBLE);
  ASSERT_FALSE(apc_scrim_manager()->GetVisible());
}
