// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_toolbar_bubble_view.h"

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service.h"
#include "chrome/browser/send_tab_to_self/send_tab_to_self_client_service_factory.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/base_window.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

namespace send_tab_to_self {

namespace {

class StubReceivingUiHandler : public ReceivingUiHandler {
 public:
  StubReceivingUiHandler() = default;
  ~StubReceivingUiHandler() override = default;

  void DisplayNewEntries(
      base::span<const SendTabToSelfEntry* const> new_entries) override {}
  void DismissEntries(base::span<const std::string> guids) override {}
};

class NavigationTextFragmentObserver : public content::WebContentsObserver {
 public:
  NavigationTextFragmentObserver()
      : creation_subscription_(
            content::RegisterWebContentsCreationCallback(base::BindRepeating(
                &NavigationTextFragmentObserver::OnWebContentsCreated,
                base::Unretained(this)))) {}
  ~NavigationTextFragmentObserver() override = default;

  void OnWebContentsCreated(content::WebContents* web_contents) {
    Observe(web_contents);
  }

  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!text_fragment_observed_) {
      text_fragment_observed_ = true;
      text_fragment_ = content::GetInternalScrollToTextFragmentForNavigation(
          navigation_handle);
    }
  }

  const std::optional<std::string>& text_fragment() const {
    return text_fragment_;
  }

 private:
  base::CallbackListSubscription creation_subscription_;
  bool text_fragment_observed_ = false;
  std::optional<std::string> text_fragment_;
};

}  // namespace

class SendTabToSelfToolbarBubbleViewTestBase : public InProcessBrowserTest {
 public:
  explicit SendTabToSelfToolbarBubbleViewTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features = {},
      const std::vector<base::test::FeatureRef>& disabled_features = {}) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  ~SendTabToSelfToolbarBubbleViewTestBase() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  SendTabToSelfSyncServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context, base::BindRepeating(
                                       [](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
                                         return std::make_unique<
                                             StubSendTabToSelfSyncService>();
                                       }));
                  SendTabToSelfClientServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(
                              [](content::BrowserContext* context)
                                  -> std::unique_ptr<KeyedService> {
                                Profile* profile =
                                    Profile::FromBrowserContext(context);
                                auto* sync_service =
                                    static_cast<StubSendTabToSelfSyncService*>(
                                        SendTabToSelfSyncServiceFactory::
                                            GetForProfile(profile));
                                return std::make_unique<
                                    SendTabToSelfClientService>(
                                    std::make_unique<StubReceivingUiHandler>(),
                                    sync_service->GetFakeSendTabToSelfModel());
                              }));
                }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    anchor_widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params(
        views::Widget::InitParams::CLIENT_OWNS_WIDGET,
        views::Widget::InitParams::TYPE_WINDOW);
    params.context = browser()->GetWindow()->GetNativeWindow();
    anchor_widget_->Init(std::move(params));
  }

  void TearDownOnMainThread() override {
    anchor_widget_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  views::Widget* anchor_widget() { return anchor_widget_.get(); }
  FakeSendTabToSelfModel* test_model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(
                   browser()->GetProfile()))
        ->GetFakeSendTabToSelfModel();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<views::Widget> anchor_widget_;
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

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarBubbleViewTest,
                       ButtonNavigatesToPage) {
  GURL url("https://www.example.com");
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid", PageContext(),
                           NavigationHistory());

  SendTabToSelfToolbarBubbleView* bubble =
      SendTabToSelfToolbarBubbleView::CreateBubble(
          *browser(), views::BubbleAnchor(anchor_widget()->GetContentsView()),
          entry);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  bubble->OpenInNewTab();
  waiter.Wait();

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_GE(tab_strip->count(), 1);
  EXPECT_EQ(url, tab_strip->GetActiveWebContents()->GetVisibleURL());

  // Verify that the model was called with the correct GUID and entry point.
  EXPECT_EQ(test_model()->last_activated_guid(), "guid");
  EXPECT_EQ(test_model()->last_activated_entry_point(),
            ShareActivatedEntryPoint::kDesktopToolbarBubble);
  EXPECT_EQ(test_model()->activated_call_count(), 1);
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarBubbleViewTest,
                       ButtonNavigatesWithScrollPosition) {
  GURL url("https://www.example.com");
  PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "target text";
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid", page_context,
                           NavigationHistory());

  NavigationTextFragmentObserver observer;

  SendTabToSelfToolbarBubbleView* bubble =
      SendTabToSelfToolbarBubbleView::CreateBubble(
          *browser(), views::BubbleAnchor(anchor_widget()->GetContentsView()),
          entry);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  bubble->OpenInNewTab();
  waiter.Wait();

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_GE(tab_strip->count(), 1);
  content::WebContents* web_contents = tab_strip->GetActiveWebContents();
  EXPECT_EQ(url, web_contents->GetVisibleURL());

  // Text fragment for scroll position syncing gets converted according to URL
  // Fragment Text Directive spec
  // (https://wicg.github.io/scroll-to-text-fragment/).
  EXPECT_EQ("target%20text", observer.text_fragment());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfToolbarBubbleViewScrollPositionDisabledTest,
                       ButtonNavigatesWithoutScrollPositionIfFeatureDisabled) {
  GURL url("https://www.example.com");
  PageContext page_context;
  page_context.scroll_position.text_fragment.text_start = "target text";
  SendTabToSelfEntry entry("guid", url, "Example", base::Time::Now(),
                           "Example Device", "sync_guid", page_context,
                           NavigationHistory());

  NavigationTextFragmentObserver observer;

  SendTabToSelfToolbarBubbleView* bubble =
      SendTabToSelfToolbarBubbleView::CreateBubble(
          *browser(), views::BubbleAnchor(anchor_widget()->GetContentsView()),
          entry);
  views::test::WidgetDestroyedWaiter waiter(bubble->GetWidget());
  bubble->OpenInNewTab();
  waiter.Wait();

  TabStripModel* tab_strip = browser()->GetTabStripModel();
  ASSERT_GE(tab_strip->count(), 1);
  content::WebContents* web_contents = tab_strip->GetActiveWebContents();
  EXPECT_EQ(url, web_contents->GetVisibleURL());

  EXPECT_FALSE(observer.text_fragment().has_value());
}

}  // namespace send_tab_to_self
