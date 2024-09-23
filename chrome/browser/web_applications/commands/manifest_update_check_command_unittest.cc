// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/manifest_update_check_command.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_contents/web_app_icon_downloader.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "components/webapps/browser/installable/installable_logging.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

static const int kUnimportantIconSize1 = 4;
static const int kUnimportantIconSize2 = 8;

namespace {
apps::FileHandlers CreateFileHandlersFromManifest(
    const std::vector<blink::mojom::ManifestFileHandlerPtr>& file_handler,
    const GURL& app_scope) {
  // Make a fake WebAppInstallInfo to extract file_handlers data.
  // TODO(b:341617121): Ideally `PopulateFileHandlerInfoFromManifest` would
  // return the file handlers directly.
  auto web_app_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_scope);
  PopulateFileHandlerInfoFromManifest(file_handler, app_scope,
                                      web_app_info.get());
  return web_app_info->file_handlers;
}
}  // namespace

class ManifestUpdateCheckUtilsTest : public testing::Test {
 public:
  ManifestUpdateCheckUtilsTest() = default;
  ManifestUpdateCheckUtilsTest(const ManifestUpdateCheckUtilsTest&) = delete;
  ManifestUpdateCheckUtilsTest& operator=(const ManifestUpdateCheckUtilsTest&) =
      delete;
  ~ManifestUpdateCheckUtilsTest() override = default;

  // Note: Keep in sync with GetDefaultManifestFileHandlers() below.
  apps::FileHandlers GetDefaultAppsFileHandlers() {
    apps::FileHandler handler;
    handler.action = GURL("http://foo.com/?plaintext");
    handler.display_name = u"Text";
    apps::FileHandler::AcceptEntry text_entry;
    text_entry.mime_type = "text/plain";
    text_entry.file_extensions = {".txt", ".md"};
    handler.accept = {text_entry};
    return {handler};
  }

  // Note: Keep in sync with GetDefaultAppsFileHandlers() above.
  std::vector<blink::mojom::ManifestFileHandlerPtr>
  GetDefaultManifestFileHandlers() {
    std::vector<blink::mojom::ManifestFileHandlerPtr> handlers;
    auto handler = blink::mojom::ManifestFileHandler::New();
    handler->action = GURL("http://foo.com/?plaintext");
    handler->name = u"Text";
    std::vector<std::u16string> extensions = {u".txt", u".md"};
    handler->accept.emplace(u"text/plain", extensions);
    handlers.push_back(std::move(handler));
    return handlers;
  }
};

// Below tests primarily test file handler comparison after conversion from
// manifest format. Basic tests like added/removed/unchanged handlers are
// also in functional tests at ManifestUpdateManagerBrowserTestWithFileHandling.
TEST_F(ManifestUpdateCheckUtilsTest, TestFileHandlersUnchanged) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_EQ(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, TestSecondFileHandlerAdded) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  auto second_handler = blink::mojom::ManifestFileHandler::New();
  second_handler->action = GURL("http://foo.com/?csv");
  second_handler->name = u"Comma-Separated Value";
  std::vector<std::u16string> extensions = {u".csv"};
  second_handler->accept.emplace(u"text/csv", extensions);
  manifest_handlers.push_back(std::move(second_handler));

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, TestFileHandlerChangedName) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->name = u"Comma-Separated Values";

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, TestFileHandlerChangedAction) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->action = GURL("/?csvtext");

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, TestFileHandlerExtraAccept) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  std::vector<std::u16string> csv_extensions = {u".csv"};
  manifest_handlers[0]->accept.emplace(u"text/csv", csv_extensions);

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, TestFileHandlerChangedMimeType) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  old_handlers[0].accept[0].mime_type = "text/csv";
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, TestFileHandlerChangedExtension) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  old_handlers[0].accept[0].file_extensions.emplace(".csv");
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateCheckUtilsTest, CompareIdentityIconBitmaps) {
  // Tests below assume there is no overlap in these values, but if
  // Install/Launcher icon sizes change, a new value for kUnimportantIconSize
  // must be selected that does not clash with it. Also check if launcher and
  // install icon are same size, because tests might need to be updated if they
  // are (browser tests especially).
  static_assert(kInstallIconSize != kLauncherIconSize, "Overlap");
  static_assert(kInstallIconSize != kUnimportantIconSize1, "Overlap");
  static_assert(kInstallIconSize != kUnimportantIconSize2, "Overlap");
  static_assert(kLauncherIconSize != kUnimportantIconSize1, "Overlap");
  static_assert(kLauncherIconSize != kUnimportantIconSize2, "Overlap");

  enum class Expectation {
    kNoChange,
    kInstallIconChanged,
    kLauncherIconChanged,
  };

  struct icon {
    int icon_size;
    SkColor icon_color;
  };

  const std::vector<icon> NoIcons;
  const SkColor starting_icon_color = SK_ColorTRANSPARENT;
  const SkColor ending_icon_color = SK_ColorRED;
  const std::vector<icon> Icon1 = {
      {kUnimportantIconSize1, starting_icon_color}};
  const std::vector<icon> Icon1Red = {
      {kUnimportantIconSize1, ending_icon_color}};
  // Another icon size.
  const std::vector<icon> Icon2 = {
      {kUnimportantIconSize2, starting_icon_color}};

  // Launcher icon (starts yellow, ends up blue).
  const SkColor starting_launcher_icon_color = SK_ColorYELLOW;
  const SkColor ending_launcher_icon_color = SK_ColorBLUE;
  const std::vector<icon> Launcher = {
      {kLauncherIconSize, starting_launcher_icon_color}};
  const std::vector<icon> LauncherBlue = {
      {kLauncherIconSize, ending_launcher_icon_color}};

  // Install icon (starts off green, ends up cyan).
  const SkColor starting_install_icon_color = SK_ColorGREEN;
  const SkColor ending_install_icon_color = SK_ColorCYAN;
  const std::vector<icon> InstallIcon = {
      {kInstallIconSize, starting_install_icon_color}};
  const std::vector<icon> InstallIconCyan = {
      {kInstallIconSize, ending_install_icon_color}};

  // Launcher and install icon together.
  const std::vector<icon> BothBefore = {
      {kLauncherIconSize, starting_launcher_icon_color},
      {kInstallIconSize, starting_install_icon_color}};
  const std::vector<icon> BothAfter = {
      {kLauncherIconSize, ending_launcher_icon_color},
      {kInstallIconSize, ending_install_icon_color}};

  // All types (Launcher, install and unimportant icon).
  const std::vector<icon> AllBefore = {
      {kUnimportantIconSize1, starting_icon_color},
      {kLauncherIconSize, starting_launcher_icon_color},
      {kInstallIconSize, starting_install_icon_color}};
  const std::vector<icon> AllAfter = {
      {kUnimportantIconSize1, ending_icon_color},
      {kLauncherIconSize, ending_launcher_icon_color},
      {kInstallIconSize, ending_install_icon_color}};

  struct {
    IconPurpose current_purpose;
    std::vector<icon> current;
    IconPurpose downloaded_purpose;
    std::vector<icon> downloaded;
    Expectation expectation;
  } test_cases[] = {
      // Test: zero icons -> zero icons:
      {IconPurpose::ANY, NoIcons, IconPurpose::ANY, NoIcons,
       Expectation::kNoChange},

      // Test: zero icons -> one icon (unimportant size) via 'any' map:
      {IconPurpose::ANY, NoIcons, IconPurpose::ANY, Icon1,
       Expectation::kNoChange},

      // Test: single icon -> zero icons:
      {IconPurpose::ANY, Icon1, IconPurpose::ANY, NoIcons,
       Expectation::kNoChange},

      // Test: single icon -> single icon (but size changes).
      {IconPurpose::ANY, Icon1, IconPurpose::ANY, Icon2,
       Expectation::kNoChange},

      // Same as above, except across maps ('any' and 'monochrome').
      {IconPurpose::ANY, Icon1, IconPurpose::MONOCHROME, Icon2,
       Expectation::kNoChange},

      // Same as above, except across maps ('maskable' and 'monochrome').
      {IconPurpose::MASKABLE, Icon1, IconPurpose::MONOCHROME, Icon2,
       Expectation::kNoChange},

      // Test: single icon (unimportant size) changes color.
      {IconPurpose::ANY, Icon1, IconPurpose::ANY, Icon1Red,
       Expectation::kNoChange},

      // Test: launcher icon changes color.
      {IconPurpose::ANY, Launcher, IconPurpose::ANY, LauncherBlue,
       Expectation::kLauncherIconChanged},

      // Test: install icon changes color.
      {IconPurpose::ANY, InstallIcon, IconPurpose::ANY, InstallIconCyan,
       Expectation::kInstallIconChanged},

      // Test: both Launcher and Install icon changes color.
      {IconPurpose::ANY, BothBefore, IconPurpose::ANY, BothAfter,
       Expectation::kInstallIconChanged},

      // Test: all types (Launcher, Install and unimportant icon) change color.
      {IconPurpose::ANY, AllBefore, IconPurpose::ANY, AllAfter,
       Expectation::kInstallIconChanged},
  };

  int i = 1;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE("Test index: " + base::NumberToString(i++));

    IconBitmaps on_disk;
    for (const auto& current_icon : test_case.current) {
      std::map<SquareSizePx, SkBitmap>* map = [&] {
        switch (test_case.current_purpose) {
          case IconPurpose::ANY:
            return &on_disk.any;
          case IconPurpose::MASKABLE:
            return &on_disk.maskable;
          case IconPurpose::MONOCHROME:
            return &on_disk.monochrome;
        }
      }();
      AddGeneratedIcon(map, current_icon.icon_size, current_icon.icon_color);
    }

    IconBitmaps downloaded;
    for (const auto& downloaded_icon : test_case.downloaded) {
      std::map<SquareSizePx, SkBitmap>* map = [&] {
        switch (test_case.downloaded_purpose) {
          case IconPurpose::ANY:
            return &downloaded.any;
          case IconPurpose::MASKABLE:
            return &downloaded.maskable;
          case IconPurpose::MONOCHROME:
            return &downloaded.monochrome;
        }
      }();
      AddGeneratedIcon(map, downloaded_icon.icon_size,
                       downloaded_icon.icon_color);
    }

    std::optional<AppIconIdentityChange> app_icon_identity_change =
        CompareIdentityIconBitmaps(on_disk, downloaded);
    switch (test_case.expectation) {
      case Expectation::kNoChange:
        EXPECT_FALSE(app_icon_identity_change.has_value());
        break;
      case Expectation::kInstallIconChanged:
        ASSERT_TRUE(app_icon_identity_change.has_value());
        ASSERT_FALSE(app_icon_identity_change->before.drawsNothing());
        ASSERT_FALSE(app_icon_identity_change->after.drawsNothing());
        EXPECT_EQ(starting_install_icon_color,
                  app_icon_identity_change->before.getColor(0, 0));
        EXPECT_EQ(ending_install_icon_color,
                  app_icon_identity_change->after.getColor(0, 0));
        break;
      case Expectation::kLauncherIconChanged:
        ASSERT_TRUE(app_icon_identity_change.has_value());
        ASSERT_FALSE(app_icon_identity_change->before.drawsNothing());
        ASSERT_FALSE(app_icon_identity_change->after.drawsNothing());
        EXPECT_EQ(starting_launcher_icon_color,
                  app_icon_identity_change->before.getColor(0, 0));
        EXPECT_EQ(ending_launcher_icon_color,
                  app_icon_identity_change->after.getColor(0, 0));
        break;
    }
  }
}

class ManifestUpdateCheckCommandTest : public WebAppTest {
 public:
  ManifestUpdateCheckCommandTest()
      : update_dialog_scope_(SetIdentityUpdateDialogActionForTesting(
            AppIdentityUpdate::kAllowed)) {}
  ManifestUpdateCheckCommandTest(const ManifestUpdateCheckCommandTest&) =
      delete;
  ManifestUpdateCheckCommandTest& operator=(
      const ManifestUpdateCheckCommandTest&) = delete;
  ~ManifestUpdateCheckCommandTest() override = default;

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());

    web_contents_manager().SetUrlLoaded(web_contents(), app_url());
  }

 protected:
  struct RunResult {
    ManifestUpdateCheckResult check_result;
    std::unique_ptr<WebAppInstallInfo> new_install_info;
  };

  RunResult RunCommandAndGetResult(const GURL& url,
                                   const webapps::AppId& app_id) {
    base::test::TestFuture<ManifestUpdateCheckResult,
                           std::unique_ptr<WebAppInstallInfo>>
        manifest_update_check_future;
    RunResult output_result;
    provider().scheduler().ScheduleManifestUpdateCheck(
        url, app_id, base::Time::Now(), web_contents()->GetWeakPtr(),
        manifest_update_check_future.GetCallback());
    EXPECT_TRUE(manifest_update_check_future.Wait());
    auto [update_result, new_install_info] =
        manifest_update_check_future.Take();
    output_result.check_result = update_result;
    output_result.new_install_info = std::move(new_install_info);
    return output_result;
  }

  webapps::AppId InstallAppFromInfo(std::unique_ptr<WebAppInstallInfo> info) {
    return test::InstallWebApp(profile(), std::move(info));
  }

  void SetupPageState(const WebAppInstallInfo& info) {
    auto& page_state = web_contents_manager().GetOrCreatePageState(app_url());

    page_state.has_service_worker = true;
    page_state.manifest_before_default_processing = GetManifestFromInfo(info);
    page_state.valid_manifest_for_web_app = true;
    page_state.error_code = webapps::InstallableStatusCode::NO_ERROR_DETECTED;
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }

  FakeWebContentsManager& web_contents_manager() {
    return static_cast<FakeWebContentsManager&>(
        provider().web_contents_manager());
  }

  GURL app_url() { return app_url_; }

 private:
  blink::mojom::ManifestPtr GetManifestFromInfo(const WebAppInstallInfo& info) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = info.start_url();
    manifest->id = GenerateManifestIdFromStartUrlOnly(info.start_url());
    manifest->scope = info.scope;
    manifest->display = info.display_mode;
    manifest->name = info.title;
    manifest->has_theme_color = info.theme_color.has_value();
    if (manifest->has_theme_color) {
      manifest->theme_color = info.theme_color.value();
    }
    return manifest;
  }

  const GURL app_url_{"http://www.foo.bar/web_apps/basic.html"};
  base::AutoReset<std::optional<AppIdentityUpdate>> update_dialog_scope_;
};

TEST_F(ManifestUpdateCheckCommandTest, Verify) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify name changes are properly propagated.
  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kStandalone;
  new_info->title = u"New Name";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpdateNeeded);
  ASSERT_TRUE(result.new_install_info);
  EXPECT_EQ(result.new_install_info->title, u"New Name");
}

TEST_F(ManifestUpdateCheckCommandTest, VerifySuccessfulScopeUpdate) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify scope changes are properly propagated.
  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = GURL("https://foo.bar.com/new_scope/");
  new_info->display_mode = DisplayMode::kStandalone;
  new_info->title = u"Foo App";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpdateNeeded);
  ASSERT_TRUE(result.new_install_info);
  EXPECT_EQ(result.new_install_info->scope,
            GURL("https://foo.bar.com/new_scope/"));
}

TEST_F(ManifestUpdateCheckCommandTest, VerifySuccessfulDisplayModeUpdate) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify display mode changes are properly propagated.
  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kMinimalUi;
  new_info->title = u"Foo App";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpdateNeeded);
  ASSERT_TRUE(result.new_install_info);
  EXPECT_EQ(result.new_install_info->display_mode, DisplayMode::kMinimalUi);
}

TEST_F(ManifestUpdateCheckCommandTest, MultiDataUpdate) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify display mode changes are properly propagated.
  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = GURL("https://foo.bar.com/new_scope/");
  new_info->display_mode = DisplayMode::kMinimalUi;
  new_info->title = u"Foo App 2";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpdateNeeded);
  ASSERT_TRUE(result.new_install_info);
  EXPECT_EQ(result.new_install_info->display_mode, DisplayMode::kMinimalUi);
  EXPECT_EQ(result.new_install_info->scope,
            GURL("https://foo.bar.com/new_scope/"));
  EXPECT_EQ(result.new_install_info->title, u"Foo App 2");
}

TEST_F(ManifestUpdateCheckCommandTest, NoAppUpdateNeeded) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  // No fields are changed, so no updates should be needed.
  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kStandalone;
  new_info->title = u"Foo App";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpToDate);
  EXPECT_FALSE(result.new_install_info);
}

TEST_F(ManifestUpdateCheckCommandTest, AppNotEligibleNoManifest) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kMinimalUi;
  new_info->title = u"Foo App";

  // SetUpPageStateForTesting() is purposefully not called here to mimic
  // the behavior that a manifest does not exist to the FakeWebAppDataRetriever.
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppNotEligible);
  EXPECT_FALSE(result.new_install_info);
}

TEST_F(ManifestUpdateCheckCommandTest, AppIdMismatch) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  // start_url changing should not move ahead with a manifest update as the
  // generated app_id is different.
  GURL start_url("https://foo.bar.com/new_app/");
  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kMinimalUi;
  new_info->title = u"Foo App";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppIdMismatch);
  EXPECT_FALSE(result.new_install_info);
}

TEST_F(ManifestUpdateCheckCommandTest, AppNameReverted) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->theme_color = SK_ColorRED;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(
      std::make_unique<WebAppInstallInfo>(install_info->Clone()));

  auto new_info = std::make_unique<WebAppInstallInfo>(install_info->Clone());
  new_info->theme_color = SK_ColorGREEN;
  new_info->title = u"Foo App 2";

  // Don't allow identity updating to test revert logic.
  base::AutoReset<std::optional<AppIdentityUpdate>> dialog_action_scope =
      SetIdentityUpdateDialogActionForTesting(AppIdentityUpdate::kSkipped);

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpdateNeeded);
  ASSERT_TRUE(result.new_install_info);
  EXPECT_EQ(result.new_install_info->theme_color, SK_ColorGREEN);
  EXPECT_EQ(result.new_install_info->title, u"Foo App");
}

TEST_F(ManifestUpdateCheckCommandTest, IconReadFromDiskFailed) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kMinimalUi;
  new_info->title = u"Foo App 2";

  base::test::TestFuture<bool> icon_delete_future;
  provider().icon_manager().DeleteData(app_id,
                                       icon_delete_future.GetCallback());
  EXPECT_TRUE(icon_delete_future.Wait());
  EXPECT_TRUE(icon_delete_future.Get<bool>());

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result,
            ManifestUpdateCheckResult::kIconReadFromDiskFailed);
  EXPECT_FALSE(result.new_install_info);
}

TEST_F(ManifestUpdateCheckCommandTest, DoNotAcceptAppUpdateDialog) {
  // Ensure we do not accept the app identity dialog for testing.
  base::AutoReset<std::optional<AppIdentityUpdate>> test_scope =
      SetIdentityUpdateDialogActionForTesting(AppIdentityUpdate::kSkipped);
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kStandalone;
  new_info->title = u"Foo App 2";

  SetupPageState(*new_info);
  RunResult result = RunCommandAndGetResult(app_url(), app_id);

  EXPECT_EQ(result.check_result, ManifestUpdateCheckResult::kAppUpToDate);
  EXPECT_FALSE(result.new_install_info);
}

TEST_F(ManifestUpdateCheckCommandTest,
       NavigationToDifferentOriginKillsCommand) {
  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kStandalone;
  new_info->title = u"New Name";

  base::test::TestFuture<void> manifest_fetch_future;
  base::test::TestFuture<ManifestUpdateCheckResult,
                         std::unique_ptr<WebAppInstallInfo>>
      manifest_update_check_future;

  SetupPageState(*new_info);

  auto& page_state = web_contents_manager().GetOrCreatePageState(app_url());
  page_state.on_manifest_fetch = manifest_fetch_future.GetCallback();
  provider().scheduler().ScheduleManifestUpdateCheck(
      app_url(), app_id, base::Time::Now(), web_contents()->GetWeakPtr(),
      manifest_update_check_future.GetCallback());

  EXPECT_TRUE(manifest_fetch_future.Wait());
  // Trigger a navigation once we reach the async installability check.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://www.abc.com"));

  EXPECT_TRUE(manifest_update_check_future.Wait());
  EXPECT_EQ(manifest_update_check_future.Get<ManifestUpdateCheckResult>(),
            ManifestUpdateCheckResult::kCancelledDueToMainFrameNavigation);
}

TEST_F(ManifestUpdateCheckCommandTest,
       MainFrameNavigationSameOriginAllowsSuccessfulCompletion) {
  // Mimic a first navigation to the app url.
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(app_url());
  const GURL different_app_with_same_origin =
      GURL("http://www.foo.bar/web_apps/basic1.html");

  auto install_info =
      WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  webapps::AppId app_id = InstallAppFromInfo(std::move(install_info));

  auto new_info = WebAppInstallInfo::CreateWithStartUrlForTesting(app_url());
  new_info->scope = app_url().GetWithoutFilename();
  new_info->display_mode = DisplayMode::kStandalone;
  new_info->title = u"New Name";

  base::test::TestFuture<void> manifest_fetch_future;
  base::test::TestFuture<ManifestUpdateCheckResult,
                         std::unique_ptr<WebAppInstallInfo>>
      manifest_update_check_future;

  SetupPageState(*new_info);

  auto& page_state = web_contents_manager().GetOrCreatePageState(app_url());
  page_state.on_manifest_fetch = manifest_fetch_future.GetCallback();
  provider().scheduler().ScheduleManifestUpdateCheck(
      app_url(), app_id, base::Time::Now(), web_contents()->GetWeakPtr(),
      manifest_update_check_future.GetCallback());

  EXPECT_TRUE(manifest_fetch_future.Wait());
  // Trigger a 2nd navigation once we reach the async installability check.
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(different_app_with_same_origin);

  EXPECT_TRUE(manifest_update_check_future.Wait());
  EXPECT_EQ(manifest_update_check_future.Get<ManifestUpdateCheckResult>(),
            ManifestUpdateCheckResult::kAppUpdateNeeded);

  EXPECT_EQ(
      manifest_update_check_future.Get<std::unique_ptr<WebAppInstallInfo>>()
          ->title,
      u"New Name");
}

}  // namespace web_app
