// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_launch_manager.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace web_app {
namespace {

class MockWebAppLaunchManager : public WebAppLaunchManager {
 public:
  explicit MockWebAppLaunchManager(Profile* profile)
      : WebAppLaunchManager(profile) {}
  MockWebAppLaunchManager(const MockWebAppLaunchManager&) = delete;
  MockWebAppLaunchManager& operator=(const MockWebAppLaunchManager&) = delete;
  ~MockWebAppLaunchManager() override = default;

  MOCK_METHOD(
      void,
      LaunchWebApplication,
      (apps::AppLaunchParams && params,
       base::OnceCallback<void(Browser* browser,
                               apps::LaunchContainer container)> callback),
      (override));
};

#if BUILDFLAG(IS_WIN)
const base::FilePath::CharType kCurrentDirectory[] =
    FILE_PATH_LITERAL("\\path");
#else
const base::FilePath::CharType kCurrentDirectory[] = FILE_PATH_LITERAL("/path");
#endif  // BUILDFLAG(IS_WIN)

const char kTestAppId[] = "test_app_id";

class WebAppLaunchManagerUnitTest : public WebAppTest {
 public:
  WebAppLaunchManagerUnitTest() = default;
  WebAppLaunchManagerUnitTest(const WebAppLaunchManagerUnitTest&) = delete;
  WebAppLaunchManagerUnitTest& operator=(const WebAppLaunchManagerUnitTest&) =
      delete;
  ~WebAppLaunchManagerUnitTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    WebAppProvider::GetForLocalAppsUnchecked(profile())->Start();
  }

 protected:
  apps::AppLaunchParams CreateLaunchParams(
      const base::CommandLine& command_line,
      const std::vector<base::FilePath>& launch_files,
      const absl::optional<GURL>& url_handler_launch_url,
      const absl::optional<GURL>& protocol_handler_launch_url) {
    apps::AppLaunchParams params(kTestAppId,
                                 apps::LaunchContainer::kLaunchContainerWindow,
                                 WindowOpenDisposition::NEW_WINDOW,
                                 apps::LaunchSource::kFromCommandLine);

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

  void ValidateOptionalGURL(const absl::optional<GURL>& actual,
                            const absl::optional<GURL>& expected) {
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

 private:
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  web_app::test::ScopedSkipMainProfileCheck skip_main_profile_check_;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
};

TEST_F(WebAppLaunchManagerUnitTest, LaunchApplication) {
  base::RunLoop run_loop;
  base::CommandLine command_line = CreateCommandLine();

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         absl::nullopt, absl::nullopt);

  testing::StrictMock<MockWebAppLaunchManager> manager(profile());
  EXPECT_CALL(manager, LaunchWebApplication(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](apps::AppLaunchParams&& params,
              base::OnceCallback<void(Browser * browser,
                                      apps::LaunchContainer container)>
                  callback) {
            ValidateLaunchParams(params, expected_results);
            run_loop.Quit();
          }));

  manager.LaunchApplication(
      kTestAppId, command_line, base::FilePath(kCurrentDirectory),
      absl::nullopt, absl::nullopt, absl::nullopt, {}, base::DoNothing());
  run_loop.Run();
}

TEST_F(WebAppLaunchManagerUnitTest, LaunchApplication_ProtocolWebPrefix) {
  base::RunLoop run_loop;
  const absl::optional<GURL> protocol_handler_launch_url(
      GURL("web+test://test"));
  base::CommandLine command_line = CreateCommandLine();

  command_line.AppendArg(protocol_handler_launch_url.value().spec());

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         absl::nullopt, protocol_handler_launch_url);
  expected_results.launch_source = apps::LaunchSource::kFromProtocolHandler;

  testing::StrictMock<MockWebAppLaunchManager> manager(profile());
  EXPECT_CALL(manager, LaunchWebApplication(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](apps::AppLaunchParams&& params,
              base::OnceCallback<void(Browser * browser,
                                      apps::LaunchContainer container)>
                  callback) {
            ValidateLaunchParams(params, expected_results);
            run_loop.Quit();
          }));

  manager.LaunchApplication(kTestAppId, command_line,
                            base::FilePath(kCurrentDirectory), absl::nullopt,
                            protocol_handler_launch_url, absl::nullopt, {},
                            base::DoNothing());
  run_loop.Run();
}

TEST_F(WebAppLaunchManagerUnitTest, LaunchApplication_ProtocolMailTo) {
  base::RunLoop run_loop;
  const absl::optional<GURL> protocol_handler_launch_url(
      GURL("mailto://test@test.com"));
  base::CommandLine command_line = CreateCommandLine();

  command_line.AppendArg(protocol_handler_launch_url.value().spec());

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, std::vector<base::FilePath>(),
                         absl::nullopt, protocol_handler_launch_url);
  expected_results.launch_source = apps::LaunchSource::kFromProtocolHandler;

  testing::StrictMock<MockWebAppLaunchManager> manager(profile());
  EXPECT_CALL(manager, LaunchWebApplication(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](apps::AppLaunchParams&& params,
              base::OnceCallback<void(Browser * browser,
                                      apps::LaunchContainer container)>
                  callback) {
            ValidateLaunchParams(params, expected_results);
            run_loop.Quit();
          }));

  manager.LaunchApplication(kTestAppId, command_line,
                            base::FilePath(kCurrentDirectory), absl::nullopt,
                            protocol_handler_launch_url, absl::nullopt, {},
                            base::DoNothing());
  run_loop.Run();
}

// Apps are not allowed to handle https:// either as protocols or as file paths.
TEST_F(WebAppLaunchManagerUnitTest, LaunchApplication_ProtocolDisallowed) {
  base::RunLoop run_loop;
  base::CommandLine command_line = CreateCommandLine();

  command_line.AppendArg("https://www.test.com/");

  apps::AppLaunchParams expected_results =
      CreateLaunchParams(command_line, {}, absl::nullopt, absl::nullopt);

  testing::StrictMock<MockWebAppLaunchManager> manager(profile());
  EXPECT_CALL(manager, LaunchWebApplication(testing::_, testing::_))
      .Times(1)
      .WillOnce(testing::Invoke(
          [&](apps::AppLaunchParams&& params,
              base::OnceCallback<void(Browser * browser,
                                      apps::LaunchContainer container)>
                  callback) {
            ValidateLaunchParams(params, expected_results);
            run_loop.Quit();
          }));

  manager.LaunchApplication(
      kTestAppId, command_line, base::FilePath(kCurrentDirectory),
      absl::nullopt, absl::nullopt, absl::nullopt, {}, base::DoNothing());
  run_loop.Run();
}

}  // namespace
}  // namespace web_app
