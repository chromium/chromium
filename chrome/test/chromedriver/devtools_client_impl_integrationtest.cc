// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <fcntl.h>
#endif

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/browser_info.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/devtools_client_impl.h"
#include "chrome/test/chromedriver/chrome/devtools_event_listener.h"
#include "chrome/test/chromedriver/chrome/navigation_tracker.h"
#include "chrome/test/chromedriver/chrome/page_load_strategy.h"
#include "chrome/test/chromedriver/chrome/page_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/target_utils.h"
#include "chrome/test/chromedriver/chrome/web_view_impl.h"
#include "chrome/test/chromedriver/chrome/web_view_info.h"
#include "chrome/test/chromedriver/chrome_launcher.h"
#include "chrome/test/chromedriver/net/pipe_builder.h"
#include "chrome/test/chromedriver/net/test_http_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::string ToString(const base::Value::Dict& node) {
  std::string json;
  base::JSONWriter::Write(node, &json);
  return json;
}

testing::AssertionResult StatusOk(const Status& status) {
  if (status.IsOk()) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

class DevToolsClientImplTest : public ::testing::Test {
 protected:
  DevToolsClientImplTest() = default;

  void SetUp() override {
    Status status{kOk};

    http_server_.Start();

    const base::CommandLine* cur_proc_cmd =
        base::CommandLine::ForCurrentProcess();
    ASSERT_TRUE(cur_proc_cmd->HasSwitch("chrome"))
        << "path to chrome is not provided";
    base::FilePath path_to_chrome = cur_proc_cmd->GetSwitchValuePath("chrome");
    ASSERT_TRUE(base::PathExists(path_to_chrome))
        << "file not found: " << path_to_chrome;
    ASSERT_TRUE(user_data_dir_temp_dir_.CreateUniqueTempDir())
        << "cannot create temp dir for user data dir";
    base::CommandLine command(path_to_chrome);
    Switches switches = GetDesktopSwitches();
    switches.SetSwitch("remote-debugging-pipe");
    switches.SetSwitch("user-data-dir",
                       user_data_dir_temp_dir_.GetPath().AsUTF8Unsafe());
    switches.AppendToCommandLine(&command);

    base::LaunchOptions options;
    pipe_builder_.SetProtocolMode(PipeBuilder::kAsciizProtocolMode);
    status = pipe_builder_.SetUpPipes(&options, &command);
    ASSERT_TRUE(StatusOk(status));
    command.AppendArg("data:,");
#if BUILDFLAG(IS_POSIX)
    options.fds_to_remap.emplace_back(1, 1);
    options.fds_to_remap.emplace_back(2, 2);
#elif BUILDFLAG(IS_WIN)
    options.stdin_handle = INVALID_HANDLE_VALUE;
    options.stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
    options.stderr_handle = GetStdHandle(STD_ERROR_HANDLE);
    options.handles_to_inherit.push_back(options.stdout_handle);
    if (options.stderr_handle != options.stdout_handle) {
      options.handles_to_inherit.push_back(options.stderr_handle);
    }
#endif
    process_ = base::LaunchProcess(command, options);
    ASSERT_TRUE(process_.IsValid());

    int exit_code;
    base::TerminationStatus chrome_status =
        base::GetTerminationStatus(process_.Handle(), &exit_code);
    ASSERT_EQ(base::TERMINATION_STATUS_STILL_RUNNING, chrome_status);

    browser_client_ = std::make_unique<DevToolsClientImpl>(
        DevToolsClientImpl::kBrowserwideDevToolsClientId, "");
  }

  void TearDown() override {
    if (browser_client_) {
      Status status =
          browser_client_->SendCommand("Browser.close", base::Value::Dict());
      EXPECT_TRUE(StatusOk(status));
    }

    web_views_.clear();
    browser_client_.reset();

    if (process_.IsValid()) {
      process_.Close();
      int exit_code;
      if (!process_.WaitForExitWithTimeout(base::Seconds(10), &exit_code)) {
        process_.Terminate(0, true);
      }
    }

    http_server_.Stop();
  }

  void SetUpConnection() {
    Status status{kOk};
    status = pipe_builder_.BuildSocket();
    ASSERT_TRUE(StatusOk(status));
    std::unique_ptr<SyncWebSocket> socket = pipe_builder_.TakeSocket();
    EXPECT_TRUE(socket->Connect(GURL()));
    status = browser_client_->SetSocket(std::move(socket));
    ASSERT_TRUE(StatusOk(status));
    Timeout timeout{base::Seconds(10)};
    base::Value::Dict result;
    status = browser_client_->SendCommandAndGetResultWithTimeout(
        "Browser.getVersion", base::Value::Dict(), &timeout, &result);
    ASSERT_TRUE(StatusOk(status));
    status = browser_info_.FillFromBrowserVersionResponse(result);
    ASSERT_TRUE(StatusOk(status));
    pipe_builder_.CloseChildEndpoints();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  TestHttpServer http_server_;
  base::ScopedTempDir user_data_dir_temp_dir_;
  base::Process process_;
  PipeBuilder pipe_builder_;
  std::list<std::unique_ptr<WebViewImpl>> web_views_;
  std::unique_ptr<DevToolsClientImpl> browser_client_;
  BrowserInfo browser_info_;
};

}  // namespace

TEST_F(DevToolsClientImplTest, DeleteGlobalJSON) {
  // During page initialization two frames are created in a row.
  // In this test we want to verity that if the global JSON object was saved in
  // the first frame the saved value will be available in the second frame as
  // well.
  Status status{kOk};
  SetUpConnection();
  Timeout timeout{base::Seconds(60)};
  status = target_utils::WaitForPage(*browser_client_, timeout);
  ASSERT_TRUE(StatusOk(status));
  WebViewsInfo views_info;
  status =
      target_utils::GetWebViewsInfo(*browser_client_, &timeout, views_info);
  ASSERT_TRUE(StatusOk(status));
  const WebViewInfo* view_info = views_info.FindFirst(WebViewInfo::kPage);
  ASSERT_NE(view_info, nullptr);
  std::unique_ptr<DevToolsClient> client;
  status = target_utils::AttachToPageTarget(*browser_client_, view_info->id,
                                            &timeout, client);
  ASSERT_TRUE(StatusOk(status));

  DevToolsClientImpl* browser_client_impl =
      static_cast<DevToolsClientImpl*>(browser_client_.get());
  DevToolsClientImpl* page_client_impl =
      static_cast<DevToolsClientImpl*>(client.get());
  status = page_client_impl->AttachTo(browser_client_impl);
  ASSERT_TRUE(StatusOk(status));

  base::Value::Dict params;
  base::Value::Dict result;
  params.Set(
      "expression",
      "window.page_label_for_test = \"starting\"; window.page_label_for_test");
  status = client->SendCommandAndGetResultWithTimeout(
      "Runtime.evaluate", params, &timeout, &result);
  const std::string* value = result.FindStringByDottedPath("result.value");
  ASSERT_THAT(value, ::testing::Pointee(testing::Eq("starting")))
      << ToString(result);
  bool frame_has_changed = false;

  Timeout navigation_timeout(base::Seconds(10), &timeout);
  while (!frame_has_changed && !navigation_timeout.IsExpired()) {
    params.clear();
    result.clear();
    params.Set("expression", "window.page_label_for_test");
    status = client->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, &navigation_timeout, &result);
    ASSERT_TRUE(StatusOk(status));
    value = result.FindStringByDottedPath("result.value");
    frame_has_changed = value == nullptr;
  }

  if (frame_has_changed) {
    // frame_has_changed is only true if the value of window.page_label_for_test
    // was set in the first frame and the page has switched to the second frame.
    // This means that the global JSON was saved in the first frame as well.
    // Now being in the second frame we can verify that the saved value of the
    // global JSON object indeed can be accessed.
    params.clear();
    result.clear();
    params.Set("expression",
               "window.cdc_adoQpoasnfa76pfcZLmcfl_JSON.stringify(321)");
    status = client->SendCommandAndGetResultWithTimeout(
        "Runtime.evaluate", params, &timeout, &result);
    EXPECT_TRUE(StatusOk(status));
    value = result.FindStringByDottedPath("result.value");
    EXPECT_THAT(value, testing::Pointee(testing::Eq("321")))
        << ToString(result);
  } else {
    VLOG(0) << "frame has not changed, the test has not effect";
  }
}
