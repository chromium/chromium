// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom-data-view.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::InvokeArgument;
using ::testing::Return;

// Measurement marks.
const char kChangeVisibleModeToReloadStartMark[] =
    "ReloadButton.ChangeVisibleModeToReload.Start";
const char kChangeVisibleModeToStopStartMark[] =
    "ReloadButton.ChangeVisibleModeToStop.Start";
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";

// Histogram names.
constexpr char kInputToReloadMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToReload.MouseRelease";
constexpr char kInputToStopMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToStop.MouseRelease";

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() = default;
  ~MockWebContentsDelegate() override = default;

  MOCK_METHOD(bool,
              HandleContextMenu,
              (content::RenderFrameHost&, const content::ContextMenuParams&),
              (override));
};

}  // namespace

// Test fixture for the ReloadButtonPageHandler class.
class ReloadButtonPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    auto* service =
        MetricsReporterService::GetFromWebContents(web_contents_.get());
    auto mock_metrics_reporter =
        std::make_unique<testing::NiceMock<MockMetricsReporter>>();
    mock_metrics_reporter_ = mock_metrics_reporter.get();
    service->SetMetricsReporterForTesting(std::move(mock_metrics_reporter));

    mock_command_updater_ =
        std::make_unique<testing::NiceMock<MockCommandUpdater>>();
    handler_ = std::make_unique<ReloadButtonPageHandler>(
        mojo::PendingReceiver<reload_button::mojom::PageHandler>(),
        page().BindAndGetRemote(), web_contents_.get(),
        mock_command_updater_.get());

    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
  }

  void TearDown() override { handler_.reset(); }

 protected:
  void ExpectMeasureAndClearMark(const std::string& start_mark,
                                 base::TimeDelta duration) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), testing::A<base::TimeTicks>(), _))
        .WillOnce(base::test::RunOnceCallback<2>(duration));
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(start_mark));
  }

  void ExpectNoMeasureCallback(const std::string& start_mark) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), testing::A<base::TimeTicks>(), _))
        .Times(1);
    // OnMeasureResultAndClearMark() calls ClearMark(). Expecting ClearMark() to
    // not be called ensures that the callback is not triggered.
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(Eq(start_mark))).Times(0);
  }

  MetricsReporterService* GetService() {
    return MetricsReporterService::GetFromWebContents(web_contents_.get());
  }

  void ClearMetricsReporter() {
    // Must be clear before setting the service to null.
    mock_metrics_reporter_ = nullptr;
    GetService()->SetMetricsReporterForTesting(nullptr);
  }

  testing::StrictMock<MockReloadButtonPage>& page() { return page_; }
  content::WebContents& web_contents() { return *web_contents_; }
  testing::NiceMock<MockCommandUpdater>& mock_command_updater() {
    return *mock_command_updater_;
  }
  testing::NiceMock<MockMetricsReporter>& mock_metrics_reporter() {
    return *mock_metrics_reporter_;
  }

  ReloadButtonPageHandler& handler() { return *handler_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  testing::StrictMock<MockReloadButtonPage> page_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<testing::NiceMock<MockCommandUpdater>> mock_command_updater_;
  raw_ptr<testing::NiceMock<MockMetricsReporter>> mock_metrics_reporter_;
  std::unique_ptr<ReloadButtonPageHandler> handler_;
  base::HistogramTester histogram_tester_;
};

// Test suite for Reload-related tests.
using ReloadButtonPageHandlerReloadTest = ReloadButtonPageHandlerTest;

// Tests that calling Reload(false, {}) executes the IDC_RELOAD command and
// records metrics.
TEST_F(ReloadButtonPageHandlerReloadTest, ReloadByMouseRelease) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_RELOAD, WindowOpenDisposition::CURRENT_TAB, testing::_));

  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  handler().Reload(/*ignore_cache=*/false, /*flags=*/{});

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling Reload(false, {}) doesn't record metrics if the start mark
// is not present.
TEST_F(ReloadButtonPageHandlerReloadTest, ReloadByMouseReleaseNoStartMark) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_RELOAD, WindowOpenDisposition::CURRENT_TAB, testing::_));
  ExpectNoMeasureCallback(kInputMouseReleaseStartMark);

  handler().Reload(/*ignore_cache=*/false, /*flags=*/{});

  histogram_tester().ExpectTotalCount(kInputToReloadMouseReleaseHistogram, 0);
}

// Tests that calling Reload(false, {middle_button}) executes the
// IDC_RELOAD with new background tab.
TEST_F(ReloadButtonPageHandlerReloadTest, ReloadWithMiddleMouseButton) {
  EXPECT_CALL(
      mock_command_updater(),
      ExecuteCommandWithDisposition(
          IDC_RELOAD, WindowOpenDisposition::NEW_BACKGROUND_TAB, testing::_));

  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  handler().Reload(
      /*ignore_cache=*/false, /*flags=*/{
          reload_button::mojom::ClickDispositionFlag::kMiddleMouseButton});

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling Reload(false, {}) does not crash if the metrics reporter
// is null.
TEST_F(ReloadButtonPageHandlerReloadTest, ReloadNoMetricsReporter) {
  // Reset the metrics reporter to null.
  ClearMetricsReporter();

  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_RELOAD, WindowOpenDisposition::CURRENT_TAB, testing::_));
  // No EXPECT_CALLs for mock_metrics_reporter_ as it is null.

  handler().Reload(/*ignore_cache=*/false, /*flags=*/{});
  // Expect no crash.
}

// Tests that calling Reload(true) executes the IDC_RELOAD_BYPASSING_CACHE
TEST_F(ReloadButtonPageHandlerReloadTest, ReloadBypassingCache) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(IDC_RELOAD_BYPASSING_CACHE,
                                            WindowOpenDisposition::CURRENT_TAB,
                                            testing::_))
      .Times(1);

  handler().Reload(/*ignore_cache=*/true, /*flags=*/{});
}

// Test suite for StopReload-related tests.
using ReloadButtonPageHandlerStopReloadTest = ReloadButtonPageHandlerTest;

// Tests that calling StopReload() executes the IDC_STOP command and records
// metrics.
TEST_F(ReloadButtonPageHandlerStopReloadTest, StopReload) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_STOP, WindowOpenDisposition::CURRENT_TAB, testing::_));
  const base::TimeDelta duration = base::Milliseconds(20);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  handler().StopReload();

  histogram_tester().ExpectUniqueTimeSample(kInputToStopMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling StopReload() doesn't record metrics if the start mark
// is not present.
TEST_F(ReloadButtonPageHandlerStopReloadTest, StopReloadNoStartMark) {
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_STOP, WindowOpenDisposition::CURRENT_TAB, testing::_));
  ExpectNoMeasureCallback(kInputMouseReleaseStartMark);

  handler().StopReload();

  histogram_tester().ExpectTotalCount(kInputToStopMouseReleaseHistogram, 0);
}

// Tests that calling StopReload() does not crash if the metrics reporter is
// null.
TEST_F(ReloadButtonPageHandlerStopReloadTest, StopReloadNoMetricsReporter) {
  ClearMetricsReporter();
  EXPECT_CALL(mock_command_updater(),
              ExecuteCommandWithDisposition(
                  IDC_STOP, WindowOpenDisposition::CURRENT_TAB, testing::_));
  // No EXPECT_CALLs for `mock_metrics_reporter()` as it is null.

  handler().StopReload();
  // Expect no crash.
}

// Tests that calling ShowContextMenu() opens the context menu.
TEST_F(ReloadButtonPageHandlerTest, TestShowContextMenu) {
  MockWebContentsDelegate delegate;
  web_contents().SetDelegate(&delegate);
  EXPECT_CALL(delegate, HandleContextMenu(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  handler().ShowContextMenu(/*offset_x=*/1, /*offset_y=*/2);
  web_contents().SetDelegate(nullptr);
}

// Tests that calling SetReloadButtonState() calls the page with the correct
// state and records metrics when loading.
TEST_F(ReloadButtonPageHandlerTest, TestSetReloadButtonStateLoading) {
  EXPECT_CALL(page(), SetReloadButtonState(true, true)).Times(1);
  EXPECT_CALL(mock_metrics_reporter(), Mark(kChangeVisibleModeToStopStartMark))
      .Times(1);

  handler().SetReloadButtonState(true, true);

  page().FlushForTesting();
}

// Tests that calling SetReloadButtonState() calls the page with the correct
// state and records metrics when not loading.
TEST_F(ReloadButtonPageHandlerTest, TestSetReloadButtonStateNotLoading) {
  EXPECT_CALL(page(), SetReloadButtonState(false, false)).Times(1);
  EXPECT_CALL(mock_metrics_reporter(),
              Mark(kChangeVisibleModeToReloadStartMark))
      .Times(1);
  handler().SetReloadButtonState(false, false);

  page().FlushForTesting();
}

// Tests that calling SetReloadButtonState() does not crash if the metrics
// reporter is null.
TEST_F(ReloadButtonPageHandlerTest, TestSetReloadButtonStateNoMetricsReporter) {
  ClearMetricsReporter();
  EXPECT_CALL(page(), SetReloadButtonState(true, true)).Times(1);
  // No EXPECT_CALLs for `mock_metrics_reporter()` as it is null.

  handler().SetReloadButtonState(true, true);

  page().FlushForTesting();
  // Expect no crash.
}
