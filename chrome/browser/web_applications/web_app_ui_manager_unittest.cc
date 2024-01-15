// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_ui_manager.h"

#include <optional>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace web_app {
namespace {

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kCurrentDirectory[] =
    FILE_PATH_LITERAL("\\path");
#else
const base::FilePath::CharType kCurrentDirectory[] = FILE_PATH_LITERAL("/path");
#endif  // BUILDFLAG(IS_WIN)

const char kTestAppId[] = "test_app_id";

class WebAppUiManagerTest : public testing::Test {
 public:
  WebAppUiManagerTest() = default;
  ~WebAppUiManagerTest() override = default;

 protected:
  void InitAppWithDisplayMode(DisplayMode display_mode) {
    auto web_app = std::make_unique<WebApp>(kTestAppId);
    web_app->SetDisplayMode(display_mode);
    if (display_mode == DisplayMode::kBrowser) {
      web_app->SetUserDisplayMode(mojom::UserDisplayMode::kBrowser);
    } else {
      web_app->SetUserDisplayMode(mojom::UserDisplayMode::kStandalone);
    }
    Registry map;
    map[kTestAppId] = std::move(web_app);
    registrar_.InitRegistry(std::move(map));
  }

  apps::AppLaunchParams CreateLaunchParams(
      const base::CommandLine& command_line,
      const std::vector<base::FilePath>& launch_files,
      const std::optional<GURL>& url_handler_launch_url,
      const std::optional<GURL>& protocol_handler_launch_url) {
    apps::AppLaunchParams params(
        kTestAppId, apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, apps::LaunchSource::kFromCommandLine);

    params.current_directory = base::FilePath(kCurrentDirectory);
    params.command_line = command_line;
    params.launch_files = launch_files;
    params.url_handler_launch_url = url_handler_launch_url;
    params.protocol_handler_launch_url = protocol_handler_launch_url;

    return params;
  }

  base::CommandLine CreateCommandLine() {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    command_line.AppendSwitchASCII(switches::kAppId, kTestAppId);
    return command_line;
  }

  void ValidateOptionalGURL(const std::optional<GURL>& actual,
                            const std::optional<GURL>& expected) {
    ASSERT_EQ(actual.has_value(), expected.has_value());
    if (actual.has_value()) {
      EXPECT_EQ(actual.value(), expected.value());
    }
  }

  void ValidateLaunchParams(const apps::AppLaunchParams& actual_results,
                            const apps::AppLaunchParams& expected_results) {
    EXPECT_EQ(actual_results.app_id, expected_results.app_id);
    EXPECT_EQ(actual_results.command_line.GetArgs(),
              expected_results.command_line.GetArgs());
    EXPECT_EQ(actual_results.current_directory,
              expected_results.current_directory);
    EXPECT_EQ(actual_results.launch_source, expected_results.launch_source);
    EXPECT_EQ(actual_results.launch_files, expected_results.launch_files);
    EXPECT_EQ(actual_results.url_handler_launch_url,
              expected_results.url_handler_launch_url);
    ValidateOptionalGURL(actual_results.url_handler_launch_url,
                         expected_results.url_handler_launch_url);
    ValidateOptionalGURL(actual_results.protocol_handler_launch_url,
                         expected_results.protocol_handler_launch_url);

    EXPECT_EQ(actual_results.protocol_handler_launch_url,
              expected_results.protocol_handler_launch_url);
  }

  WebAppRegistrarMutable registrar_{nullptr};
};

TEST_F(WebAppUiManagerTest, DefaultParamsTab) {
  base::CommandLine command_line = CreateCommandLine();

  InitAppWithDisplayMode(DisplayMode::kBrowser);

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         /*url_handler_launch_url=*/std::nullopt,
                         /*protocol_handler_launch_url=*/std::nullopt);

  ValidateLaunchParams(
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          kTestAppId, command_line, base::FilePath(kCurrentDirectory),
          /*url_handler_launch_url=*/std::nullopt,
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{}),
      expected_results);
}

TEST_F(WebAppUiManagerTest, DefaultParamsStandalone) {
  InitAppWithDisplayMode(DisplayMode::kStandalone);
  base::CommandLine command_line = CreateCommandLine();

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         /*url_handler_launch_url=*/std::nullopt,
                         /*protocol_handler_launch_url=*/std::nullopt);

  ValidateLaunchParams(
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          kTestAppId, command_line, base::FilePath(kCurrentDirectory),
          /*url_handler_launch_url=*/std::nullopt,
          /*protocol_handler_launch_url=*/std::nullopt,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{}),
      expected_results);
}

TEST_F(WebAppUiManagerTest, ProtocolHandlerUrl) {
  InitAppWithDisplayMode(DisplayMode::kStandalone);
  const std::optional<GURL> protocol_handler_launch_url(
      GURL("web+test://test"));
  base::CommandLine command_line = CreateCommandLine();
  command_line.AppendArg(protocol_handler_launch_url.value().spec());

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         std::nullopt, protocol_handler_launch_url);
  expected_results.launch_source = apps::LaunchSource::kFromProtocolHandler;

  ValidateLaunchParams(
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          kTestAppId, command_line, base::FilePath(kCurrentDirectory),
          /*url_handler_launch_url=*/std::nullopt, protocol_handler_launch_url,
          /*file_launch_url=*/std::nullopt, /*launch_files=*/{}),
      expected_results);
}

TEST_F(WebAppUiManagerTest, LaunchApplication_ProtocolMailTo) {
  InitAppWithDisplayMode(DisplayMode::kStandalone);
  const std::optional<GURL> protocol_handler_launch_url(
      GURL("mailto://test@test.com"));
  base::CommandLine command_line = CreateCommandLine();

  command_line.AppendArg(protocol_handler_launch_url.value().spec());

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         std::nullopt, protocol_handler_launch_url);
  expected_results.launch_source = apps::LaunchSource::kFromProtocolHandler;

  ValidateLaunchParams(
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          kTestAppId, command_line, base::FilePath(kCurrentDirectory),
          std::nullopt, protocol_handler_launch_url, std::nullopt, {}),
      expected_results);
}

// Apps are not allowed to handle https:// either as protocols or as file paths.
TEST_F(WebAppUiManagerTest, LaunchApplication_ProtocolDisallowed) {
  InitAppWithDisplayMode(DisplayMode::kStandalone);
  base::CommandLine command_line = CreateCommandLine();

  command_line.AppendArg("https://www.test.com/");

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, {}, std::nullopt, std::nullopt);

  ValidateLaunchParams(
      WebAppUiManager::CreateAppLaunchParamsWithoutWindowConfig(
          kTestAppId, command_line, base::FilePath(kCurrentDirectory),
          std::nullopt, std::nullopt, std::nullopt, {}),
      expected_results);
}

}  // namespace
}  // namespace web_app
