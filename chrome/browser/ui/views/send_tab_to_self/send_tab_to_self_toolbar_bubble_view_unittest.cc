// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/test_send_tab_to_self_model.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"

namespace send_tab_to_self {

namespace {

using MockNavigateCallback = base::MockCallback<base::OnceCallback<
    base::WeakPtr<content::NavigationHandle>(NavigateParams*)>>;

MATCHER_P3(NavParamsMatch, url, disposition, window_action, "") {
  return arg && arg->url == url && arg->disposition == disposition &&
         arg->window_action == window_action;
}

MATCHER_P(NavParamsScrollToTextMatch, expected_text, "") {
  return arg && arg->internal_scroll_to_text_fragment == expected_text;
}

class StubSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  explicit StubSendTabToSelfSyncService(SendTabToSelfModel* model)
      : model_(model) {}
  ~StubSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override { return model_; }

 private:
  raw_ptr<SendTabToSelfModel> model_;
};

class StubReceivingUiHandler : public ReceivingUiHandler {
 public:
  StubReceivingUiHandler() = default;
  ~StubReceivingUiHandler() override = default;

  void DisplayNewEntries(
      const std::vector<const SendTabToSelfEntry*>& new_entries) override {}
  void DismissEntries(const std::vector<std::string>& guids) override {}
};

}  // namespace

class SendTabToSelfToolbarBubbleViewTestBase : public TestWithBrowserView {
 public:
  SendTabToSelfToolbarBubbleViewTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~SendTabToSelfToolbarBubbleViewTestBase() override = default;

  void SetUp() override {
    TestWithBrowserView::SetUp();

    anchor_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.context = GetContext();
    anchor_widget_->Init(std::move(params));
  }

  void TearDown() override {
    anchor_widget_.reset();
    TestWithBrowserView::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    TestingProfile::TestingFactories factories =
        TestWithBrowserView::GetTestingFactories();
    factories.emplace_back(
        SendTabToSelfSyncServiceFactory::GetInstance(),
        base::BindLambdaForTesting([&](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>(&test_model_);
        }));
    factories.emplace_back(
        SendTabToSelfClientServiceFactory::GetInstance(),
        base::BindLambdaForTesting([&](content::BrowserContext* context)
                                       -> std::unique_ptr<KeyedService> {
          return std::make_unique<SendTabToSelfClientService>(
              std::make_unique<StubReceivingUiHandler>(), &test_model_);
        }));
    return factories;
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }
  TestSendTabToSelfModel& test_model() { return test_model_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<views::Widget> anchor_widget_;
  TestSendTabToSelfModel test_model_;
};

class SendTabToSelfToolbarBubbleViewTest
    : public SendTabToSelfToolbarBubbleViewTestBase {
 public:
  SendTabToSelfToolbarBubbleViewTest()
      : SendTabToSelfToolbarBubbleViewTestBase(
            {kSendTabToSelfPropagateScrollPosition}) {}
};

class SendTabToSelfToolbarBubbleViewScrollPositionDisabledTest
    : public SendTabToSelfToolbarBubbleViewTestBase {
 public:
  SendTabToSelfToolbarBubbleViewScrollPositionDisabledTest()
      : SendTabToSelfToolbarBubbleViewTestBase(
            {},
            {kSendTabToSelfPropagateScrollPosition}) {}
};

TEST_F(SendTabToSelfToolbarBubbleViewTest, ButtonNavigatesToPage) {
  GURL url("https://www.example.com");
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid", PageContext());

  MockNavigateCallback mock_callback;
  EXPECT_CALL(mock_callback,
              Run(NavParamsMatch(url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                 NavigateParams::WindowAction::kShowWindow)));

  SendTabToSelfToolbarBubbleView* bubble =
      SendTabToSelfToolbarBubbleView::CreateBubble(
          *browser(), anchor_widget()->GetContentsView(), entry,
          mock_callback.Get());
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  bubble->OpenInNewTab();
  waiter.Wait();
}

TEST_F(SendTabToSelfToolbarBubbleViewTest, ButtonNavigatesWithScrollPosition) {
  GURL url("https://www.example.com");
  PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "target text";
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid", page_context);

  MockNavigateCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(NavParamsScrollToTextMatch("target%20text")));

  SendTabToSelfToolbarBubbleView* bubble =
      SendTabToSelfToolbarBubbleView::CreateBubble(
          *browser(), anchor_widget()->GetContentsView(), entry,
          mock_callback.Get());
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  bubble->OpenInNewTab();
  waiter.Wait();
}

TEST_F(SendTabToSelfToolbarBubbleViewScrollPositionDisabledTest,
       ButtonNavigatesWithoutScrollPositionIfFeatureDisabled) {
  GURL url("https://www.example.com");
  PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "target text";
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid", page_context);

  MockNavigateCallback mock_callback;
  EXPECT_CALL(mock_callback, Run(NavParamsScrollToTextMatch(std::nullopt)));

  SendTabToSelfToolbarBubbleView* bubble =
      SendTabToSelfToolbarBubbleView::CreateBubble(
          *browser(), anchor_widget()->GetContentsView(), entry,
          mock_callback.Get());
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  bubble->OpenInNewTab();
  waiter.Wait();
}

}  // namespace send_tab_to_self
