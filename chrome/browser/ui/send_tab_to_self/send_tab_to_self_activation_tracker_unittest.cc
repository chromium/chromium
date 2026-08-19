// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_activation_tracker.h"

#include <memory>

#include "chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/metrics_util.h"
#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

class SendTabToSelfActivationTrackerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SendTabToSelfActivationTrackerTest() = default;
  ~SendTabToSelfActivationTrackerTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    SendTabToSelfSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            &SendTabToSelfActivationTrackerTest::BuildStubSyncService,
            base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> BuildStubSyncService(
      content::BrowserContext* context) {
    return std::make_unique<StubSendTabToSelfSyncService>();
  }

  FakeSendTabToSelfModel* model() {
    return static_cast<StubSendTabToSelfSyncService*>(
               SendTabToSelfSyncServiceFactory::GetForProfile(profile()))
        ->GetFakeSendTabToSelfModel();
  }
};

TEST_F(SendTabToSelfActivationTrackerTest, RecordDesktopTabStrip) {
  content::WebContents* contents = web_contents();
  contents->WasHidden();

  // Attach the activation tracker with a specific GUID.
  SendTabToSelfActivationTracker::CreateForWebContents(contents, "test_guid");

  // Show the WebContents to trigger activation recording.
  contents->WasShown();

  // Verify that the model was called with the correct GUID and entry point.
  EXPECT_EQ(model()->last_activated_guid(), "test_guid");
  EXPECT_EQ(model()->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabStrip);
  EXPECT_EQ(model()->activated_call_count(), 1);
}

TEST_F(SendTabToSelfActivationTrackerTest, RecordDesktopToast) {
  content::WebContents* contents = web_contents();
  contents->WasHidden();

  // Attach the activation tracker with a specific GUID.
  SendTabToSelfActivationTracker::CreateForWebContents(contents, "toast_guid");

  // Mark the tab as opened via toast.
  SendTabToSelfActivationTracker::SetEntryOpenedViaToast(contents);

  // Show the WebContents to trigger activation recording.
  contents->WasShown();

  // Verify that the model was called with the correct GUID and entry point.
  EXPECT_EQ(model()->last_activated_guid(), "toast_guid");
  EXPECT_EQ(model()->last_activated_entry_point(),
            ShareActivatedEntryPoint::kDesktopToast);
  EXPECT_EQ(model()->activated_call_count(), 1);
}

TEST_F(SendTabToSelfActivationTrackerTest, RecordOnlyOnce) {
  content::WebContents* contents = web_contents();
  contents->WasHidden();

  // Attach the activation tracker with a specific GUID.
  SendTabToSelfActivationTracker::CreateForWebContents(contents, "once_guid");

  // Show, hide, and show again to verify it only records once.
  contents->WasShown();
  contents->WasHidden();
  contents->WasShown();

  // Verify that the model was only called once (tracker self-destructs on first
  // activation).
  EXPECT_EQ(model()->last_activated_guid(), "once_guid");
  EXPECT_EQ(model()->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabStrip);
  EXPECT_EQ(model()->activated_call_count(), 1);
}

TEST_F(SendTabToSelfActivationTrackerTest, TabDestroyedWithoutShowing) {
  // Create a scoped WebContents that starts hidden.
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  contents->WasHidden();
  // Attach the activation tracker with a specific GUID.
  SendTabToSelfActivationTracker::CreateForWebContents(contents.get(),
                                                       "destroy_guid");

  // Destroy the WebContents without ever showing/activating it.
  contents.reset();

  // Verify that the model was called with the correct GUID and entry point.
  EXPECT_EQ(model()->last_activated_guid(), "destroy_guid");
  EXPECT_EQ(model()->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabOrBrowserClosedWithoutActivation);
  EXPECT_EQ(model()->activated_call_count(), 1);
}

// Tests that destroying a WebContents due to tab discarding (e.g. Memory Saver)
// does not record a false tab abandonment activation metric.
TEST_F(SendTabToSelfActivationTrackerTest, DoNotRecordAbandonmentOnDiscard) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  contents->WasHidden();
  SendTabToSelfActivationTracker::CreateForWebContents(contents.get(),
                                                       "discard_guid");

  // Mark the WebContents as discarded before destroying it.
  contents->SetWasDiscarded(true);
  contents.reset();

  // Verify that abandonment was not recorded.
  EXPECT_EQ(model()->activated_call_count(), 0);
}

// Tests that restoring a tab in the foreground from session restore or discard
// marks the entry as activated via the tab strip.
TEST_F(SendTabToSelfActivationTrackerTest,
       RestoreFromExtraDataInForegroundRecordsActivation) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  contents->WasShown();

  std::map<std::string, std::string> extra_data = {
      {"send_tab_to_self.entry_guid", "restored_guid"}};
  SendTabToSelfActivationTracker::RestoreFromExtraData(contents.get(),
                                                       extra_data);

  EXPECT_EQ(model()->last_activated_guid(), "restored_guid");
  EXPECT_EQ(model()->last_activated_entry_point(),
            ShareActivatedEntryPoint::kTabStrip);
  EXPECT_EQ(model()->activated_call_count(), 1);
}

}  // namespace send_tab_to_self
