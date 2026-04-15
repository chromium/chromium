// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/features.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/sync/test/fake_data_type_controller_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/mojom/link_to_text/link_to_text.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace send_tab_to_self {

namespace {

class StubSendTabToSelfSyncService : public SendTabToSelfSyncService {
 public:
  StubSendTabToSelfSyncService() : fake_delegate_(syncer::SEND_TAB_TO_SELF) {}
  ~StubSendTabToSelfSyncService() override = default;

  SendTabToSelfModel* GetSendTabToSelfModel() override { return &model_fake_; }

  base::WeakPtr<syncer::DataTypeControllerDelegate> GetControllerDelegate()
      override {
    return fake_delegate_.GetWeakPtr();
  }

  FakeSendTabToSelfModel* GetModelFake() { return &model_fake_; }

 protected:
  syncer::FakeDataTypeControllerDelegate fake_delegate_;
  FakeSendTabToSelfModel model_fake_;
};

std::unique_ptr<KeyedService> BuildStubSyncService(
    content::BrowserContext* context) {
  return std::make_unique<StubSendTabToSelfSyncService>();
}

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

class SendTabToSelfBubbleControllerBrowserTest : public InProcessBrowserTest {
 public:
  SendTabToSelfBubbleControllerBrowserTest() {
    feature_list_.InitAndEnableFeature(kSendTabToSelfPropagateScrollPosition);
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&SendTabToSelfBubbleControllerBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildStubSyncService));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleControllerBrowserTest,
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

  StubSendTabToSelfSyncService* sync_service =
      static_cast<StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::CreateOrGetFromWebContents(web_contents);
  // Increase the timeout for tests to avoid flakiness on slow bots.
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  base::HistogramTester histogram_tester;
  controller->OnDeviceSelected("device_1");
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

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleControllerBrowserTest,
                       ScrollPositionPropagated_EmptyPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL test_url = embedded_test_server()->GetURL("/empty.html");

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, test_url));

  StubSendTabToSelfSyncService* sync_service =
      static_cast<StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::CreateOrGetFromWebContents(web_contents);
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  base::HistogramTester histogram_tester;
  controller->OnDeviceSelected("device_1");
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

IN_PROC_BROWSER_TEST_F(SendTabToSelfBubbleControllerBrowserTest,
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

  StubSendTabToSelfSyncService* sync_service =
      static_cast<StubSendTabToSelfSyncService*>(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser()->profile()));
  ASSERT_TRUE(sync_service);

  SendTabToSelfBubbleController* controller =
      SendTabToSelfBubbleController::CreateOrGetFromWebContents(web_contents);
  controller->SetSelectorGenerationTimeoutForTesting(base::Seconds(2));

  TestSendTabToSelfModelObserver observer(
      sync_service->GetSendTabToSelfModel());

  base::HistogramTester histogram_tester;
  controller->OnDeviceSelected("device_1");
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

}  // namespace send_tab_to_self
