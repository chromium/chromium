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
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/webui_toolbar/testing/toy_browser.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/browser_apis/browser_controls/browser_controls_api.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
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

// Measurement marks.
constexpr char kInputMouseReleaseStartMark[] =
    "ReloadButton.Input.MouseRelease.Start";

// Histogram names.
constexpr char kInputToReloadMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToReload.MouseRelease";
constexpr char kInputToStopMouseReleaseHistogram[] =
    "InitialWebUI.ReloadButton.InputToStop.MouseRelease";

class MockBrowserControlsServiceDelegate
    : public BrowserControlsService::BrowserControlsServiceDelegate {
 public:
  MockBrowserControlsServiceDelegate() = default;

  MOCK_METHOD(void, PermitLaunchUrl, (), (override));
};

// This is really an integration test. We provide a faked environment so that we
// can have an easily predictable sealed environment to exercise the
// interactions between our service and dependencies. To validate our
// integration with "real" browser services, we should utilize browser tests.
class BrowserControlsServiceTest : public ::testing::Test {
 public:
  void SetUp() override {
    service_ = std::make_unique<BrowserControlsService>(
        mojo::PendingReceiver<mojom::BrowserControlsService>(),
        toy_browser_.GetAdapter(), &metrics_reporter_, &delegate_);
  }

  void TearDown() override { service_.reset(); }

 protected:
  void ExpectMeasureAndClearMark(const std::string& start_mark,
                                 base::TimeDelta duration) {
    EXPECT_CALL(mock_metrics_reporter(),
                Measure(Eq(start_mark), ::testing::A<base::TimeTicks>(), _))
        .WillOnce(base::test::RunOnceCallback<2>(duration));
    EXPECT_CALL(mock_metrics_reporter(), ClearMark(start_mark));
  }

  ::testing::NiceMock<MockMetricsReporter>& mock_metrics_reporter() {
    return metrics_reporter_;
  }

  ToyBrowser& toy_browser() { return toy_browser_; }
  BrowserControlsService& service() { return *service_; }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }
  MockBrowserControlsServiceDelegate& delegate() { return delegate_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  testing::ToyBrowser toy_browser_;
  ::testing::NiceMock<MockMetricsReporter> metrics_reporter_;
  std::unique_ptr<BrowserControlsService> service_;
  base::HistogramTester histogram_tester_;
  MockBrowserControlsServiceDelegate delegate_;
};

// Test suite for Reload-related tests.
using BrowserControlsServiceReloadTest = BrowserControlsServiceTest;

// Tests that calling Reload(false, {}) executes the IDC_RELOAD command and
// records metrics.
TEST_F(BrowserControlsServiceReloadTest, ReloadByMouseRelease) {
  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  service().ReloadFromClick(/*bypass_cache=*/false, /*click_flags=*/{});

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

// Tests that calling Reload(false, {middle_button}) executes the
// IDC_RELOAD with new background tab.
TEST_F(BrowserControlsServiceReloadTest, ReloadWithMiddleMouseButton) {
  const base::TimeDelta duration = base::Milliseconds(10);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  service().ReloadFromClick(
      /*bypass_cache=*/false,
      /*click_flags=*/{mojom::ClickDispositionFlag::kMiddleMouseButton});

  EXPECT_EQ(IDC_RELOAD, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::NEW_BACKGROUND_TAB,
            toy_browser().received_commands().back().disposition);

  histogram_tester().ExpectUniqueTimeSample(kInputToReloadMouseReleaseHistogram,
                                            duration, 1);
}

TEST_F(BrowserControlsServiceReloadTest, ReloadBypassingCache) {
  service().ReloadFromClick(/*bypass_cache=*/true, /*click_flags=*/{});

  EXPECT_EQ(1ul, toy_browser().received_commands().size());
  EXPECT_EQ(IDC_RELOAD_BYPASSING_CACHE,
            toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);
}

// Test suite for StopLoad-related tests.
using BrowserControlsServiceStopLoadTest = BrowserControlsServiceTest;

// Tests that calling StopLoad() executes the IDC_STOP command and records
// metrics.
TEST_F(BrowserControlsServiceStopLoadTest, StopLoad) {
  const base::TimeDelta duration = base::Milliseconds(20);
  ExpectMeasureAndClearMark(kInputMouseReleaseStartMark, duration);

  service().StopLoad();

  EXPECT_EQ(IDC_STOP, toy_browser().received_commands().back().command_id);
  EXPECT_EQ(WindowOpenDisposition::CURRENT_TAB,
            toy_browser().received_commands().back().disposition);

  histogram_tester().ExpectUniqueTimeSample(kInputToStopMouseReleaseHistogram,
                                            duration, 1);
}
}  // namespace

}  // namespace browser_controls_api
