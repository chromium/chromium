// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/reload_button/reload_button_page_handler.h"

#include <memory>

#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/metrics_reporter/mock_metrics_reporter.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"

namespace {

// Mock implementation of the mojom::Page interface.
class MockPage : public reload_button::mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  // Returns a PendingRemote to this mock implementation.
  mojo::PendingRemote<reload_button::mojom::Page> BindAndGetRemote() {
    CHECK(!receiver_.is_bound());
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, SetReloadButtonState, (bool, bool), (override));

  mojo::Receiver<reload_button::mojom::Page> receiver_{this};
};

class MockCommandUpdater : public CommandUpdater {
 public:
  MockCommandUpdater() = default;
  ~MockCommandUpdater() override = default;

  MOCK_METHOD(bool, SupportsCommand, (int id), (const, override));
  MOCK_METHOD(bool, IsCommandEnabled, (int id), (const, override));
  MOCK_METHOD(bool,
              ExecuteCommand,
              (int id, base::TimeTicks time_stamp),
              (override));
  MOCK_METHOD(bool,
              ExecuteCommandWithDisposition,
              (int id,
               WindowOpenDisposition disposition,
               base::TimeTicks time_stamp),
              (override));
  MOCK_METHOD(void,
              AddCommandObserver,
              (int id, CommandObserver* observer),
              (override));
  MOCK_METHOD(void,
              RemoveCommandObserver,
              (int id, CommandObserver* observer),
              (override));
  MOCK_METHOD(void,
              RemoveCommandObserver,
              (CommandObserver * observer),
              (override));
  MOCK_METHOD(bool, UpdateCommandEnabled, (int id, bool enabled), (override));
};

}  // namespace

// Test fixture for the ReloadButtonPageHandler class.
class ReloadButtonPageHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    MetricsReporterService::GetFromWebContents(web_contents_.get());
    handler_ = std::make_unique<ReloadButtonPageHandler>(
        mojo::PendingReceiver<reload_button::mojom::PageHandler>(),
        page_.BindAndGetRemote(), web_contents_.get(), &mock_command_updater_);
    handler_->SetMetricsReporterForTesting(&mock_metrics_reporter_);
  }

  void TearDown() override { handler_.reset(); }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  testing::StrictMock<MockPage> page_;
  std::unique_ptr<content::WebContents> web_contents_;
  testing::NiceMock<MockCommandUpdater> mock_command_updater_;
  testing::NiceMock<MockMetricsReporter> mock_metrics_reporter_;
  std::unique_ptr<ReloadButtonPageHandler> handler_;
};

// Tests that calling Reload(false) executes the IDC_RELOAD command.
TEST_F(ReloadButtonPageHandlerTest, TestReload) {
  EXPECT_CALL(mock_command_updater_, ExecuteCommand(IDC_RELOAD, testing::_));
  EXPECT_CALL(mock_metrics_reporter_, Mark(testing::_)).Times(1);
  handler_->Reload(false);
}

// Tests that calling Reload(true) executes the IDC_RELOAD_BYPASSING_CACHE
TEST_F(ReloadButtonPageHandlerTest, TestReloadBypassingCache) {
  EXPECT_CALL(mock_command_updater_,
              ExecuteCommand(IDC_RELOAD_BYPASSING_CACHE, testing::_));
  handler_->Reload(true);
}

// Tests that calling StopReload() executes the IDC_STOP command.
TEST_F(ReloadButtonPageHandlerTest, TestStopReload) {
  EXPECT_CALL(mock_command_updater_, ExecuteCommand(IDC_STOP, testing::_));
  handler_->StopReload();
}
