// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/assistant_side_panel_coordinator_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_side_panel_coordinator.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class MockAssistantSidePanelObserver
    : public AssistantSidePanelCoordinator::Observer {
 public:
  MockAssistantSidePanelObserver() = default;
  ~MockAssistantSidePanelObserver() override = default;

  MOCK_METHOD(void, OnHidden, (), (override));
};

class AssistantSidePanelCoordinatorImplTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    base::test::ScopedFeatureList features;
    features.InitWithFeatures({features::kUnifiedSidePanel}, {});

    TestWithBrowserView::SetUp();
    AddTab(browser_view()->browser(), GURL("http://foo1.com"));
    browser_view()->browser()->tab_strip_model()->ActivateTabAt(0);

    assistant_side_panel_coordinator_ = AssistantSidePanelCoordinator::Create(
        browser_view()->GetActiveWebContents());
    web_contents_ = browser_view()->GetActiveWebContents();
  }

  void TearDown() override {
    if (assistant_side_panel_coordinator_)
      assistant_side_panel_coordinator_.reset();
    TestWithBrowserView::TearDown();
  }

  AssistantSidePanelCoordinator* assistant_side_panel_coordinator() {
    return assistant_side_panel_coordinator_.get();
  }
  content::WebContents* web_contents() { return web_contents_; }

 private:
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AssistantSidePanelCoordinator>
      assistant_side_panel_coordinator_;
};

TEST_F(AssistantSidePanelCoordinatorImplTest, GetSetView) {
  AssistantSidePanelCoordinator* coordinator =
      assistant_side_panel_coordinator();
  views::View* view = coordinator->SetView(std::make_unique<views::View>());
  EXPECT_EQ(view, coordinator->GetView());
}

TEST_F(AssistantSidePanelCoordinatorImplTest, OverwritePreviouslySetView) {
  AssistantSidePanelCoordinator* coordinator =
      assistant_side_panel_coordinator();
  coordinator->SetView(std::make_unique<views::View>());
  views::View* view = coordinator->SetView(std::make_unique<views::View>());
  EXPECT_EQ(view, coordinator->GetView());
}

TEST_F(AssistantSidePanelCoordinatorImplTest, RemoveView) {
  AssistantSidePanelCoordinator* coordinator =
      assistant_side_panel_coordinator();
  coordinator->SetView(std::make_unique<views::View>());
  coordinator->RemoveView();
  EXPECT_EQ(nullptr, coordinator->GetView());
}

TEST_F(AssistantSidePanelCoordinatorImplTest,
       AssistantSidePanelAlreadyRegisteredReturnsNullptr) {
  std::unique_ptr<AssistantSidePanelCoordinator> coordinator =
      AssistantSidePanelCoordinator::Create(
          browser_view()->GetActiveWebContents());
  EXPECT_EQ(nullptr, coordinator);
}

TEST_F(AssistantSidePanelCoordinatorImplTest,
       AssistantSidePanelPropagatesOnHidden) {
  // Since we are testing the implementation here, we can safely perform this
  // cast.
  AssistantSidePanelCoordinatorImpl* coordinator =
      static_cast<AssistantSidePanelCoordinatorImpl*>(
          assistant_side_panel_coordinator());
  auto observer = std::make_unique<MockAssistantSidePanelObserver>();
  coordinator->AddObserver(observer.get());

  // Only a total of 1 call is expected.
  EXPECT_CALL(*observer, OnHidden).Times(1);
  coordinator->OnEntryHidden(nullptr);

  coordinator->RemoveObserver(observer.get());
  // This call does not get registered.
  coordinator->OnEntryHidden(nullptr);
}
