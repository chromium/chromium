// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_toolbar/browser_controls_service.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/waap/waap_ui_metrics_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/webui_toolbar/testing/toy_browser.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace browser_controls_api {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using testing::ToyBrowser;

#if BUILDFLAG(IS_MAC)
constexpr mojom::EventDispositionFlag control_or_meta_disposition =
    browser_controls_api::mojom::EventDispositionFlag::kMetaKeyDown;
#else
constexpr mojom::EventDispositionFlag control_or_meta_disposition =
    browser_controls_api::mojom::EventDispositionFlag::kControlKeyDown;
#endif  // BUILDFLAG(IS_MAC)

class MockBrowserControlsServiceDelegate
    : public BrowserControlsService::BrowserControlsServiceDelegate {
 public:
  MockBrowserControlsServiceDelegate() = default;

  MOCK_METHOD(void, PermitLaunchUrl, (), (override));
  // Mocks the baseline query for the unit test pipeline.
  MOCK_METHOD(base::TimeTicks, GetNavigationStartTicks, (), (const, override));
};

// This is really an integration test. We provide a faked environment so that we
// can have an easily predictable sealed environment to exercise the
// interactions between our service and dependencies. To validate our
// integration with "real" browser services, we should utilize browser tests.
class BrowserControlsServiceTest : public ChromeRenderViewHostTestHarness {
 public:
  BrowserControlsServiceTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Fast-forward the mock clock to ensure we operate on positive, stable
    // TimeTicks and avoid underflow issues when subtracting offsets in
    // validation tests.
    task_environment()->FastForwardBy(base::Seconds(100));

    service_ = std::make_unique<BrowserControlsService>(
        mojo::PendingReceiver<mojom::BrowserControlsService>(),
        toy_browser_.GetAdapter(), &delegate_, main_rfh());

    // Configure the mock delegate to return a stable baseline (10 seconds ago)
    // to reconstruct absolute timestamps during validation tests.
    EXPECT_CALL(delegate_, GetNavigationStartTicks())
        .WillRepeatedly(
            ::testing::Return(base::TimeTicks::Now() - base::Seconds(10)));
  }

  void TearDown() override {
    service_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  ToyBrowser& toy_browser() { return toy_browser_; }
  BrowserControlsService& service() { return *service_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockBrowserControlsServiceDelegate& delegate() { return delegate_; }

 private:
  testing::ToyBrowser toy_browser_;
  std::unique_ptr<BrowserControlsService> service_;
  base::HistogramTester histogram_tester_;
  MockBrowserControlsServiceDelegate delegate_;
};

// Test suite for Reload-related tests.
using BrowserControlsServiceReloadTest = BrowserControlsServiceTest;

// Tests that calling Reload(false, {}) executes the IDC_RELOAD command.
TEST_F(BrowserControlsServiceReloadTest, ReloadByMouseRelease) {
  auto metadata = mojom::ReloadInteractionMetadata::New();
  metadata->input_type = mojom::ReloadInputType::kMouseRelease;

  std::ignore = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata));

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
}

// Tests that calling Reload(false, {middle_button}) executes the
// IDC_RELOAD with new background tab.
TEST_F(BrowserControlsServiceReloadTest, ReloadWithMiddleMouseButton) {

  auto metadata = mojom::ReloadInteractionMetadata::New();
  metadata->input_type = mojom::ReloadInputType::kMouseRelease;

  std::ignore = service().ReloadFromClick(
      /*bypass_cache=*/false,
      /*click_flags=*/{mojom::EventDispositionFlag::kMiddleMouseButton},
      std::move(metadata));

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

TEST_F(BrowserControlsServiceReloadTest, ReloadBypassingCache) {
  std::ignore =
      service().ReloadFromClick(/*bypass_cache=*/true, /*click_flags=*/{});

  EXPECT_EQ(size_t{1}, toy_browser().received_commands().size());
  EXPECT_EQ(IDC_RELOAD_BYPASSING_CACHE,
            toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

TEST_F(BrowserControlsServiceReloadTest, TelemetryMouseRelease) {
  auto now = base::TimeTicks::Now();
  auto nav_start = now - base::Seconds(1);
  EXPECT_CALL(delegate(), GetNavigationStartTicks())
      .WillRepeatedly(::testing::Return(nav_start));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();

  auto metadata = mojom::ReloadInteractionMetadata::New();
  base::TimeDelta target_duration = base::Milliseconds(100);
  metadata->interaction_time_offset = base::Seconds(1) - target_duration;
  metadata->input_type = mojom::ReloadInputType::kMouseRelease;

  std::ignore = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata));

  histogram_tester().ExpectUniqueTimeSample(
      "InitialWebUI.ReloadButton.InteractionToReload.MouseRelease",
      target_duration, 1);
  histogram_tester().ExpectUniqueTimeSample(
      "InitialWebUI.ReloadButton.InteractionToReload", target_duration, 1);
}

TEST_F(BrowserControlsServiceReloadTest, TelemetryKeyPress) {
  auto now = base::TimeTicks::Now();
  auto nav_start = now - base::Seconds(1);
  EXPECT_CALL(delegate(), GetNavigationStartTicks())
      .WillRepeatedly(::testing::Return(nav_start));

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();

  auto metadata = mojom::ReloadInteractionMetadata::New();
  base::TimeDelta target_duration = base::Milliseconds(200);
  metadata->interaction_time_offset = base::Seconds(1) - target_duration;
  metadata->input_type = mojom::ReloadInputType::kKeyPress;

  std::ignore = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata));

  histogram_tester().ExpectUniqueTimeSample(
      "InitialWebUI.ReloadButton.InteractionToReload.KeyPress", target_duration,
      1);
  histogram_tester().ExpectUniqueTimeSample(
      "InitialWebUI.ReloadButton.InteractionToReload", target_duration, 1);
}

struct ValidationParam {
  base::TimeDelta nav_start_offset;
  base::TimeDelta interaction_offset;
  bool simulate_activation;
};

class BrowserControlsServiceValidationTest
    : public BrowserControlsServiceTest,
      public ::testing::WithParamInterface<ValidationParam> {};

INSTANTIATE_TEST_SUITE_P(
    All,
    BrowserControlsServiceValidationTest,
    ::testing::Values(
        // Negative Offset
        ValidationParam{.nav_start_offset = base::Seconds(10),
                        .interaction_offset = base::Milliseconds(-100),
                        .simulate_activation = true},
        // No User Activation
        ValidationParam{.nav_start_offset = base::Milliseconds(500),
                        .interaction_offset = base::Milliseconds(100),
                        .simulate_activation = false},
        // Future Timestamp
        ValidationParam{.nav_start_offset = base::Seconds(1),
                        .interaction_offset = base::Milliseconds(1100),
                        .simulate_activation = true},
        // Stale Timestamp (older than 10s threshold)
        ValidationParam{.nav_start_offset = base::Seconds(12),
                        .interaction_offset = base::Milliseconds(500),
                        .simulate_activation = true},
        // Overflow / Max TimeDelta (Potential Integer Wrap-around)
        ValidationParam{.nav_start_offset = base::Seconds(1),
                        .interaction_offset = base::TimeDelta::Max(),
                        .simulate_activation = true}));

TEST_P(BrowserControlsServiceValidationTest, InvalidInputs) {
  const ValidationParam& param = GetParam();

  auto now = base::TimeTicks::Now();
  EXPECT_CALL(delegate(), GetNavigationStartTicks())
      .WillRepeatedly(::testing::Return(now - param.nav_start_offset));

  if (param.simulate_activation) {
    content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();
  }

  auto metadata = mojom::ReloadInteractionMetadata::New();
  metadata->interaction_time_offset = param.interaction_offset;
  metadata->input_type = mojom::ReloadInputType::kMouseRelease;

  auto result = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata));
  ASSERT_TRUE(result.has_value());

  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload.MouseRelease", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload", 0);

  // Assert reload was not blocked.
  EXPECT_FALSE(toy_browser().received_commands().empty());
  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
}

TEST_F(BrowserControlsServiceReloadTest,
       Validation_MissingNavigationStartTicks) {
  std::unique_ptr<content::WebContents> new_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::RenderFrameHost* new_rfh = new_web_contents->GetPrimaryMainFrame();

  auto temp_service = std::make_unique<BrowserControlsService>(
      mojo::PendingReceiver<mojom::BrowserControlsService>(),
      toy_browser().GetAdapter(), &delegate(), new_rfh);

  EXPECT_CALL(delegate(), GetNavigationStartTicks())
      .WillRepeatedly(::testing::Return(base::TimeTicks()));

  content::RenderFrameHostTester::For(new_rfh)->SimulateUserActivation();

  auto metadata = mojom::ReloadInteractionMetadata::New();
  metadata->interaction_time_offset = base::Milliseconds(100);
  metadata->input_type = mojom::ReloadInputType::kMouseRelease;

  auto result = temp_service->ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata));
  ASSERT_TRUE(result.has_value());

  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload.MouseRelease", 0);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload", 0);
}

TEST_F(BrowserControlsServiceReloadTest, Validation_DuplicateTimestamp) {
  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();

  auto now = base::TimeTicks::Now();
  auto nav_start = now - base::Seconds(1);
  EXPECT_CALL(delegate(), GetNavigationStartTicks())
      .WillRepeatedly(::testing::Return(nav_start));

  auto metadata = mojom::ReloadInteractionMetadata::New();
  metadata->interaction_time_offset = base::Milliseconds(900);
  metadata->input_type = mojom::ReloadInputType::kMouseRelease;

  auto result1 = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata));
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(size_t{1}, toy_browser().received_commands().size());

  content::RenderFrameHostTester::For(main_rfh())->SimulateUserActivation();

  auto metadata2 = mojom::ReloadInteractionMetadata::New();
  metadata2->interaction_time_offset = base::Milliseconds(900);
  metadata2->input_type = mojom::ReloadInputType::kMouseRelease;

  auto result2 = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, std::move(metadata2));
  ASSERT_TRUE(result2.has_value());

  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload.MouseRelease", 1);
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload", 1);

  // Assert reload was not blocked.
  EXPECT_EQ(size_t{2}, toy_browser().received_commands().size());
  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
}

// Tests that dropped touch events (represented by null metadata) trigger
// the reload, but do NOT record any mouse release metrics.
TEST_F(BrowserControlsServiceReloadTest, ReloadNullMetadataDroppedTouch) {
  // Call ReloadFromClick with null metadata.
  auto result = service().ReloadFromClick(
      /*bypass_cache=*/false, /*click_flags=*/{}, nullptr);
  ASSERT_TRUE(result.has_value());

  // Assert reload command was triggered.
  EXPECT_EQ(size_t{1}, toy_browser().received_commands().size());
  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);

  // Assert that no mouse release metrics are recorded.
  histogram_tester().ExpectTotalCount(
      "InitialWebUI.ReloadButton.InteractionToReload", 0);
}
// Test suite for StopLoad-related tests.
using BrowserControlsServiceStopLoadTest = BrowserControlsServiceTest;

// Tests that calling StopLoad() executes the IDC_STOP command.
TEST_F(BrowserControlsServiceStopLoadTest, StopLoad) {
  std::ignore = service().StopLoad();

  EXPECT_EQ(IDC_STOP, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Back() with CURRENT_TAB executes the IDC_BACK command
// with CURRENT_TAB.
TEST_F(BrowserControlsServiceTest, Back_CurrentTab) {
  std::ignore = service().Back({});
  EXPECT_EQ(IDC_BACK, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Back() with kMiddleMouseButton executes the IDC_BACK
// command with NEW_BACKGROUND_TAB.
TEST_F(BrowserControlsServiceTest, Back_MiddleClick) {
  std::ignore = service().Back(
      {browser_controls_api::mojom::EventDispositionFlag::kMiddleMouseButton});
  EXPECT_EQ(IDC_BACK, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Back() with the platform's background tab modifier
// executes the IDC_BACK command with NEW_BACKGROUND_TAB. On macOS, Ctrl+Click
// opens a context menu, so we test Meta+Click instead.
TEST_F(BrowserControlsServiceTest, Back_MetaOrCtrlClick) {
  std::ignore = service().Back({control_or_meta_disposition});
  EXPECT_EQ(IDC_BACK, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Back() with kShiftKeyDown executes the IDC_BACK command
// with NEW_WINDOW.
TEST_F(BrowserControlsServiceTest, Back_ShiftClick) {
  std::ignore = service().Back(
      {browser_controls_api::mojom::EventDispositionFlag::kShiftKeyDown});
  EXPECT_EQ(IDC_BACK, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_WINDOW,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Forward() by default executes the IDC_FORWARD
// command with CURRENT_TAB.
TEST_F(BrowserControlsServiceTest, Forward_CurrentTab) {
  std::ignore = service().Forward({});
  EXPECT_EQ(IDC_FORWARD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Forward() with kMiddleMouseButton executes the IDC_FORWARD
// command with NEW_BACKGROUND_TAB.
TEST_F(BrowserControlsServiceTest, Forward_MiddleClick) {
  std::ignore = service().Forward(
      {browser_controls_api::mojom::EventDispositionFlag::kMiddleMouseButton});
  EXPECT_EQ(IDC_FORWARD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Forward() with the platform's background tab modifier
// executes the IDC_FORWARD command with NEW_BACKGROUND_TAB. On macOS,
// Ctrl+Click opens a context menu, so we test Meta+Click instead.
TEST_F(BrowserControlsServiceTest, Forward_MetaOrCtrlClick) {
  std::ignore = service().Forward({control_or_meta_disposition});
  EXPECT_EQ(IDC_FORWARD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Forward() with kShiftKeyDown executes the IDC_FORWARD
// command with NEW_WINDOW.
TEST_F(BrowserControlsServiceTest, Forward_ShiftClick) {
  std::ignore = service().Forward(
      {browser_controls_api::mojom::EventDispositionFlag::kShiftKeyDown});
  EXPECT_EQ(IDC_FORWARD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_WINDOW,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling BackButtonHovered()
TEST_F(BrowserControlsServiceTest, BackButtonHovered) {
  EXPECT_FALSE(toy_browser().is_back_button_hovered());
  std::ignore = service().BackButtonHovered();
  EXPECT_TRUE(toy_browser().is_back_button_hovered());
}

// Tests that calling NavigateHome() by default executes the IDC_HOME
// command with CURRENT_TAB.
TEST_F(BrowserControlsServiceTest, NavigateHome_CurrentTab) {
  std::ignore = service().NavigateHome({});
  EXPECT_EQ(IDC_HOME, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling NavigateHome() with kMiddleMouseButton executes the
// IDC_HOME command with NEW_BACKGROUND_TAB.
TEST_F(BrowserControlsServiceTest, NavigateHome_MiddleClick) {
  std::ignore = service().NavigateHome(
      {browser_controls_api::mojom::EventDispositionFlag::kMiddleMouseButton});
  EXPECT_EQ(IDC_HOME, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling NavigateHome() with the platform's background tab modifier
// executes the IDC_HOME command with NEW_BACKGROUND_TAB. On macOS,
// Ctrl+Click opens a context menu, so we test Meta+Click instead.
TEST_F(BrowserControlsServiceTest, NavigateHome_MetaOrCtrlClick) {
  std::ignore = service().NavigateHome({control_or_meta_disposition});
  EXPECT_EQ(IDC_HOME, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling NavigateHome() with the platform's background tab modifier
// and the Shift key executes the IDC_HOME command with NEW_FOREGROUND_TAB.
TEST_F(BrowserControlsServiceTest, NavigateHome_MetaOrCtrlShiftClick) {
  std::ignore = service().NavigateHome({
      browser_controls_api::mojom::EventDispositionFlag::kShiftKeyDown,
      control_or_meta_disposition,
  });
  EXPECT_EQ(IDC_HOME, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_FOREGROUND_TAB,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling NavigateHome() with kShiftKeyDown executes the IDC_HOME
// command with NEW_WINDOW.
TEST_F(BrowserControlsServiceTest, NavigateHome_ShiftClick) {
  std::ignore = service().NavigateHome(
      {browser_controls_api::mojom::EventDispositionFlag::kShiftKeyDown});
  EXPECT_EQ(IDC_HOME, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_WINDOW,
            toy_browser().received_commands().back().disposition);
}

// Tests that calling Navigate() executes the navigate command on the adapter.
TEST_F(BrowserControlsServiceTest, Navigate) {
  GURL test_url("https://www.example.test/");
  std::ignore = service().Navigate(test_url);
  ASSERT_EQ(1u, toy_browser().received_urls().size());
  EXPECT_EQ(test_url, toy_browser().received_urls().back());
}

TEST_F(BrowserControlsServiceTest, NavigateText) {
  std::string text = "testing search query";
  std::ignore = service().NavigateText(text);
  ASSERT_EQ(1u, toy_browser().received_navigate_texts().size());
  EXPECT_EQ(text, toy_browser().received_navigate_texts().back());
}

TEST_F(BrowserControlsServiceTest, EventFlagsConversion) {
  using browser_controls_api::mojom::EventDispositionFlag;
  constexpr auto kTests = std::to_array<std::pair<EventDispositionFlag, int>>(
      {{EventDispositionFlag::kMiddleMouseButton, ui::EF_MIDDLE_MOUSE_BUTTON},
       {EventDispositionFlag::kAltKeyDown, ui::EF_ALT_DOWN},
       {EventDispositionFlag::kMetaKeyDown, ui::EF_COMMAND_DOWN},
       {EventDispositionFlag::kShiftKeyDown, ui::EF_SHIFT_DOWN},
       {EventDispositionFlag::kControlKeyDown, ui::EF_CONTROL_DOWN},
       {EventDispositionFlag::kAltGrKeyDown, ui::EF_ALTGR_DOWN}});

  for (const auto& testcase : kTests) {
    SCOPED_TRACE(testcase.first);
    // Test individual flag.
    {
      auto result = BrowserControlsService::ToUiEventFlags({testcase.first});
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(*result, testcase.second);
    }

    // Also in combinations.
    {
      auto result = BrowserControlsService::ToUiEventFlags(
          {testcase.first, EventDispositionFlag::kAltKeyDown});
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(*result, testcase.second | ui::EF_ALT_DOWN);
    }

    {
      auto result = BrowserControlsService::ToUiEventFlags(
          {testcase.first, EventDispositionFlag::kAltKeyDown,
           EventDispositionFlag::kMiddleMouseButton});
      ASSERT_TRUE(result.has_value());
      EXPECT_EQ(*result,
                testcase.second | ui::EF_ALT_DOWN | ui::EF_MIDDLE_MOUSE_BUTTON);
    }

    // Now throw in kUnspecified.
    {
      auto result = BrowserControlsService::ToUiEventFlags(
          {testcase.first, EventDispositionFlag::kUnspecified});
      ASSERT_FALSE(result.has_value());
      EXPECT_EQ(mojo_base::mojom::Code::kInvalidArgument, result.error()->code);
    }
  }
}

}  // namespace

}  // namespace browser_controls_api
