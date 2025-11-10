// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
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

const char kChangeVisibleModeToReloadStartMark[] =
    "ReloadButton.ChangeVisibleModeToReload.Start";
const char kChangeVisibleModeToStopStartMark[] =
    "ReloadButton.ChangeVisibleModeToStop.Start";
const char kReloadForMouseReleaseEndMark[] =
    "ReloadButton.Reload.MouseRelease.End";
const char kStopForMouseReleaseEndMark[] = "ReloadButton.Stop.MouseRelease.End";

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
        page_.BindAndGetRemote(), web_contents_.get(),
        mock_command_updater_.get());

    page_.FlushForTesting();
    testing::Mock::VerifyAndClearExpectations(&page_);
  }

  void TearDown() override { handler_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  testing::StrictMock<MockReloadButtonPage> page_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_window_;
  std::unique_ptr<testing::NiceMock<MockCommandUpdater>> mock_command_updater_;
  raw_ptr<testing::NiceMock<MockMetricsReporter>> mock_metrics_reporter_;
  std::unique_ptr<ReloadButtonPageHandler> handler_;
};

// Tests that calling Reload(false) executes the IDC_RELOAD command.
TEST_F(ReloadButtonPageHandlerTest, TestReload) {
  EXPECT_CALL(*mock_command_updater_, ExecuteCommand(IDC_RELOAD, testing::_));
  EXPECT_CALL(*mock_metrics_reporter_, Mark(kReloadForMouseReleaseEndMark))
      .Times(1);

  handler_->Reload(false);
}

// Tests that calling Reload(true) executes the IDC_RELOAD_BYPASSING_CACHE
TEST_F(ReloadButtonPageHandlerTest, TestReloadBypassingCache) {
  EXPECT_CALL(*mock_command_updater_,
              ExecuteCommand(IDC_RELOAD_BYPASSING_CACHE, testing::_))
      .Times(1);
  EXPECT_CALL(*mock_metrics_reporter_, Mark(kReloadForMouseReleaseEndMark))
      .Times(1);

  handler_->Reload(true);
}

// Tests that calling StopReload() executes the IDC_STOP command.
TEST_F(ReloadButtonPageHandlerTest, TestStopReload) {
  EXPECT_CALL(*mock_command_updater_, ExecuteCommand(IDC_STOP, testing::_))
      .Times(1);
  EXPECT_CALL(*mock_metrics_reporter_, Mark(kStopForMouseReleaseEndMark))
      .Times(1);

  handler_->StopReload();
}

// Tests that calling ShowContextMenu() opens the context menu.
TEST_F(ReloadButtonPageHandlerTest, TestShowContextMenu) {
  MockWebContentsDelegate delegate;
  web_contents_->SetDelegate(&delegate);
  EXPECT_CALL(delegate, HandleContextMenu(testing::_, testing::_))
      .WillOnce(testing::Return(true));

  handler_->ShowContextMenu(/*offset_x=*/1, /*offset_y=*/2);
  web_contents_->SetDelegate(nullptr);
}

// Tests that calling SetReloadButtonState() calls the page with the correct
// state and records metrics when loading.
TEST_F(ReloadButtonPageHandlerTest, SetReloadButtonStateLoading) {
  EXPECT_CALL(page_, SetReloadButtonState(true, true)).Times(1);
  EXPECT_CALL(*mock_metrics_reporter_, Mark(kChangeVisibleModeToStopStartMark))
      .Times(1);

  handler_->SetReloadButtonState(true, true);

  page_.FlushForTesting();
}

// Tests that calling SetReloadButtonState() calls the page with the correct
// state and records metrics when not loading.
TEST_F(ReloadButtonPageHandlerTest, SetReloadButtonStateNotLoading) {
  EXPECT_CALL(page_, SetReloadButtonState(false, false)).Times(1);
  EXPECT_CALL(*mock_metrics_reporter_,
              Mark(kChangeVisibleModeToReloadStartMark))
      .Times(1);
  handler_->SetReloadButtonState(false, false);

  page_.FlushForTesting();
}
