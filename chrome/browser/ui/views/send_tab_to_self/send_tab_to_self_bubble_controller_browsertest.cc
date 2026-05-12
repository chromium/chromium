// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_context_menu_delegate.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/toasts/toast_view.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"

namespace send_tab_to_self {

namespace {

class TestSendTabToSelfModelObserver : public SendTabToSelfModelObserver {
 public:
  explicit TestSendTabToSelfModelObserver(SendTabToSelfModel* model) {
    observation_.Observe(model);
  }
  ~TestSendTabToSelfModelObserver() override = default;

  void EntryAddedLocally(const SendTabToSelfEntry* entry) override {
    last_added_entry_ = std::make_unique<SendTabToSelfEntry>(*entry);
    if (entry_added_callback_) {
      std::move(entry_added_callback_).Run();
    }
  }

  void EntriesAddedRemotely(
      const std::vector<const SendTabToSelfEntry*>& entries) override {}
  void EntriesRemovedRemotely(const std::vector<std::string>& guids) override {}
  void SendTabToSelfModelLoaded() override {}

  const SendTabToSelfEntry* last_added_entry() const {
    return last_added_entry_.get();
  }

  void WaitForEntryAdded() {
    if (last_added_entry_) {
      return;
    }
    base::RunLoop run_loop;
    entry_added_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  std::unique_ptr<SendTabToSelfEntry> last_added_entry_;
  base::OnceClosure entry_added_callback_;
  base::ScopedObservation<SendTabToSelfModel, SendTabToSelfModelObserver>
      observation_{this};
};

}  // namespace

class SendTabToSelfBubbleControllerBrowserTest : public SigninBrowserTestBase {
 public:
  SendTabToSelfBubbleControllerBrowserTest() = default;

  void SetUpInProcessBrowserTestFixture() override {
    SigninBrowserTestBase::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SendTabToSelfBubbleControllerBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    SigninBrowserTestBase::OnWillCreateBrowserContextServices(context);
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<StubSendTabToSelfSyncService>();
        }));
  }

  void ExpectToastShown(ToastId expected_id,
                        int message_id,
                        const std::u16string& replacement = u"") {
    ToastController* toast_controller =
        browser()->GetFeatures().toast_controller();

    EXPECT_EQ(toast_controller->GetCurrentToastId(), expected_id);
    toasts::ToastView* toast_view = toast_controller->GetToastViewForTesting();
    ASSERT_TRUE(toast_view);

    std::u16string expected_text =
        replacement.empty()
            ? l10n_util::GetStringUTF16(message_id)
            : l10n_util::GetStringFUTF16(message_id, replacement);

    EXPECT_EQ(toast_view->label_for_testing()->GetText(), expected_text);
  }

  StubSendTabToSelfSyncService* GetStubSyncService() {
    return static_cast<StubSendTabToSelfSyncService*>(
        SendTabToSelfSyncServiceFactory::GetForProfile(browser()->profile()));
  }

 protected:
  base::CallbackListSubscription create_services_subscription_;
};

class SendTabToSelfPostSendToastBrowserTest
    : public SendTabToSelfBubbleControllerBrowserTest {
 public:
  SendTabToSelfPostSendToastBrowserTest() {
    feature_list_.InitWithFeatures({kSendTabToSelfPostSendToast}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfPostSendToastBrowserTest,
                       BubbleShowsToast) {
  GURL test_url("about:blank");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  sync_service->GetFakeSendTabToSelfModel()->SetTargetDeviceInfoSortedList(
      {TargetDeviceInfo("device_name_1", "device_1",
                        syncer::DeviceInfo::FormFactor::kDesktop,
                        base::Time::Now())});

  controller->OnDeviceSelected("device_1", "device_name_1");
  observer.WaitForEntryAdded();

  ExpectToastShown(ToastId::kSendTabToSelfSuccess,
                   IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
                   u"device_name_1");
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfPostSendToastBrowserTest,
                       BubbleShowsThrottledToast) {
  GURL test_url("about:blank");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service =
      static_cast<StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  sync_service->GetFakeSendTabToSelfModel()->SetTargetDeviceInfoSortedList(
      {TargetDeviceInfo("device_name_1", "device_1",
                        syncer::DeviceInfo::FormFactor::kDesktop,
                        base::Time::Now())});
  sync_service->GetFakeSendTabToSelfModel()->SetSendResult(
      SendTabToSelfResult::kSuccessThrottled);

  controller->OnDeviceSelected("device_1", "device_name_1");
  observer.WaitForEntryAdded();

  ExpectToastShown(ToastId::kSendTabToSelfSuccessThrottled,
                   IDS_SEND_TAB_TO_SELF_POST_SEND_THROTTLED_TOAST,
                   u"device_name_1");
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfPostSendToastBrowserTest,
                       ContextMenuShowsToast) {
  GURL test_url(
      "data:text/html;charset=utf-8,<html><body><p>Test</p></body></html>");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  sync_service->GetFakeSendTabToSelfModel()->SetTargetDeviceInfoSortedList(
      {TargetDeviceInfo("device_name_1", "device_1",
                        syncer::DeviceInfo::FormFactor::kDesktop,
                        base::Time::Now())});

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  SendTabToSelfContextMenuDelegate delegate(web_contents);
  delegate.ExecuteCommand(IDC_CONTENT_CONTEXT_SEND_TAB_TO_SELF_DEVICE1, 0);

  observer.WaitForEntryAdded();

  ExpectToastShown(ToastId::kSendTabToSelfSuccess,
                   IDS_SEND_TAB_TO_SELF_POST_SEND_SUCCESS_TOAST,
                   u"device_name_1");
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfPostSendToastBrowserTest,
                       BubbleShowsFailureToast) {
  GURL test_url("about:blank");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  // Simulate failure by making the model not ready.
  sync_service->GetFakeSendTabToSelfModel()->SetIsReady(false);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);

  controller->OnDeviceSelected("device_1", "device_name_1");

  // Verify that the failure toast is shown.
  ExpectToastShown(
      ToastId::kSendTabToSelfFailure,
      IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_FAILURE_MESSAGE);
}

class SendTabToSelfPostSendToastDisabledBrowserTest
    : public SendTabToSelfBubbleControllerBrowserTest {
 public:
  SendTabToSelfPostSendToastDisabledBrowserTest() {
    feature_list_.InitAndDisableFeature(kSendTabToSelfPostSendToast);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfPostSendToastDisabledBrowserTest,
                       BubbleShowsFailureNotification) {
  GURL test_url("about:blank");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  // Simulate failure by making the model not ready.
  sync_service->GetFakeSendTabToSelfModel()->SetIsReady(false);

  // Use NotificationDisplayServiceTester to monitor notifications.
  NotificationDisplayServiceTester notification_tester(browser()->profile());

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);

  controller->OnDeviceSelected("device_1", "device_name_1");

  // Verify that a notification is shown.
  std::vector<message_center::Notification> notifications =
      notification_tester.GetDisplayedNotificationsForType(
          NotificationHandler::Type::SHARING);
  ASSERT_EQ(notifications.size(), 1u);
  EXPECT_EQ(
      notifications[0].title(),
      l10n_util::GetStringUTF16(
          IDS_MESSAGE_NOTIFICATION_SEND_TAB_TO_SELF_CONFIRMATION_FAILURE_TITLE));
}

class SendTabToSelfScrollPositionBrowserTest
    : public SendTabToSelfBubbleControllerBrowserTest {
 public:
  SendTabToSelfScrollPositionBrowserTest() {
    feature_list_.InitWithFeatures({kSendTabToSelfPropagateScrollPosition}, {});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfScrollPositionBrowserTest,
                       ScrollPositionPropagated_HappyPath) {
  ASSERT_TRUE(embedded_test_server()->Start());
  // Using a page with significant content ensures the renderer can generate
  // a selector for the center of the viewport.
  GURL test_url(
      "data:text/html;charset=utf-8,<html><body>"
      "<p style='text-align: center'>This is some test content "
      "that is long enough to be selected by the text fragment "
      "generator.</p></body></html>");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);
  // Increase the timeout for tests to avoid flakiness on slow bots.
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  base::HistogramTester histogram_tester;
  controller->OnDeviceSelected("device_1", "device_name_1");
  observer.WaitForEntryAdded();

  // Test that the entry was added with the correct URL.
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            observer.last_added_entry()->GetURL());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationOutcome",
      ScrollPositionGenerationOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationTime", 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.ScrollPosition.SelectorLength", 1);

  // The scroll position should be populated from the successful extraction.
  EXPECT_FALSE(
      observer.last_added_entry()->GetPageContext().scroll_position.IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfScrollPositionBrowserTest,
                       ScrollPositionPropagated_EmptyPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);
  // Increase the timeout for tests to avoid flakiness on slow bots.
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  base::HistogramTester histogram_tester;
  controller->OnDeviceSelected("device_1", "device_name_1");
  observer.WaitForEntryAdded();

  // Test that the entry was added with the correct URL.
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            observer.last_added_entry()->GetURL());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationOutcome",
      ScrollPositionGenerationOutcome::kLinkGenerationError, 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationTime", 1);

  // The scroll position should be empty because the page has no content.
  EXPECT_TRUE(
      observer.last_added_entry()->GetPageContext().scroll_position.IsEmpty());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfScrollPositionBrowserTest,
                       ScrollPositionPropagated_ScrolledPage) {
  // Use a data URL to avoid external dependencies. The page is long enough to
  // require scrolling.
  GURL test_url(
      "data:text/html;charset=utf-8,<html><body>"
      "<div style='height: 2000px'>Spacer Top</div>"
      "<p id='text' style='text-align: center'>Some interesting text</p>"
      "<div style='height: 2000px'>Spacer Bottom</div>"
      "</body></html>");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  // Scroll to the content so it's precisely in the center of the viewport.
  EXPECT_TRUE(content::ExecJs(
      web_contents,
      "new Promise(r => {"
      "  document.getElementById('text').scrollIntoView("
      "      {behavior: 'instant', block: 'center', inline: 'center'});"
      "  requestAnimationFrame(() => "
      "    requestAnimationFrame(r)"
      "  );"
      "});"));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);
  // Increase the timeout for tests to avoid flakiness on slow bots.
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  base::HistogramTester histogram_tester;
  controller->OnDeviceSelected("device_1", "device_name_1");
  observer.WaitForEntryAdded();

  // Test that the entry was added with the correct URL.
  EXPECT_EQ(web_contents->GetLastCommittedURL(),
            observer.last_added_entry()->GetURL());

  histogram_tester.ExpectUniqueSample(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationOutcome",
      ScrollPositionGenerationOutcome::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.ScrollPosition.GenerationTime", 1);
  histogram_tester.ExpectTotalCount(
      "Sharing.SendTabToSelf.ScrollPosition.SelectorLength", 1);

  // The scroll position should be populated since the text is now in the
  // viewport.
  EXPECT_FALSE(
      observer.last_added_entry()->GetPageContext().scroll_position.IsEmpty());
  // Verify that the generated selector matches the expected text.
  EXPECT_EQ(observer.last_added_entry()
                ->GetPageContext()
                .scroll_position.text_fragment.text_start,
            "interesting");
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleControllerBrowserTest,
                       ShowPromoBubble) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL("about:blank")));

  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);
  sync_service->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);

  controller->ShowBubble();

  EXPECT_TRUE(controller->IsBubbleShown());
  EXPECT_NE(nullptr, controller->send_tab_to_self_bubble_view());
}

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleControllerBrowserTest,
                       PromoBubbleAccept_OpensDiceSignInTab) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, GURL("about:blank")));

  // Trigger the 'Offer Sign-In' state by overriding the entry point display
  // reason.
  StubSendTabToSelfSyncService* sync_service = GetStubSyncService();
  ASSERT_TRUE(sync_service);
  sync_service->SetEntryPointDisplayReason(
      EntryPointDisplayReason::kOfferSignIn);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::GetOrCreateForWebContents(web_contents);
  controller->ShowBubble();

  ASSERT_TRUE(controller->IsBubbleShown());
  SendTabToSelfBubbleView* bubble = controller->send_tab_to_self_bubble_view();
  ASSERT_NE(nullptr, bubble);

  views::test::WidgetDestroyedWaiter destroyed_waiter(bubble->GetWidget());
  bubble->AcceptDialog();
  destroyed_waiter.Wait();

  // Verify that the bubble delegate accepts and successfully dismisses/closes
  // the dialog.
  EXPECT_FALSE(controller->IsBubbleShown());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace send_tab_to_self
