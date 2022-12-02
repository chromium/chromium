// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "chrome/browser/web_applications/commands/manifest_update_data_fetch_command.h"
#include "chrome/browser/web_applications/manifest_update_utils.h"
#include "chrome/browser/web_applications/test/fake_data_retriever.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test.h"
#include "chrome/browser/web_applications/web_app_callback_app_identity.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_icon_generator.h"
#include "chrome/browser/web_applications/web_app_icon_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"

namespace web_app {

static const int kUnimportantIconSize1 = 4;
static const int kUnimportantIconSize2 = 8;

class ManifestUpdateDataFetchUtilsTest : public testing::Test {
 public:
  ManifestUpdateDataFetchUtilsTest() = default;
  ManifestUpdateDataFetchUtilsTest(const ManifestUpdateDataFetchUtilsTest&) =
      delete;
  ManifestUpdateDataFetchUtilsTest& operator=(
      const ManifestUpdateDataFetchUtilsTest&) = delete;
  ~ManifestUpdateDataFetchUtilsTest() override = default;

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

  std::vector<apps::IconInfo> GenerateIconInfosFrom(
      const IconBitmaps& downloaded) {
    std::vector<apps::IconInfo> result;
    for (const auto& entry : downloaded.any) {
      apps::IconInfo icon_info(GURL(), entry.first);
      icon_info.purpose = apps::IconInfo::Purpose::kAny;
      result.push_back(icon_info);
    }
    for (const auto& entry : downloaded.maskable) {
      apps::IconInfo icon_info(GURL(), entry.first);
      icon_info.purpose = apps::IconInfo::Purpose::kMaskable;
      result.push_back(icon_info);
    }
    for (const auto& entry : downloaded.monochrome) {
      apps::IconInfo icon_info(GURL(), entry.first);
      icon_info.purpose = apps::IconInfo::Purpose::kMonochrome;
      result.push_back(icon_info);
    }
    return result;
  }

  std::string DiffResultsToString(uint32_t diff) {
    std::string result = "";
    if (diff & NO_CHANGE_DETECTED)
      result += "NO_CHANGE_DETECTED, ";
    if (diff & MISMATCHED_IMAGE_SIZES)
      result += "MISMATCHED_IMAGE_SIZES, ";
    if (diff & ONE_OR_MORE_ICONS_CHANGED)
      result += "ONE_OR_MORE_ICONS_CHANGED, ";
    if (diff & LAUNCHER_ICON_CHANGED)
      result += "LAUNCHER_ICON_CHANGED, ";
    if (diff & INSTALL_ICON_CHANGED)
      result += "INSTALL_ICON_CHANGED, ";
    if (diff & UNIMPORTANT_ICON_CHANGED)
      result += "UNIMPORTANT_ICON_CHANGED, ";
    return result;
  }
};

// Below tests primarily test file handler comparison after conversion from
// manifest format. Basic tests like added/removed/unchanged handlers are
// also in functional tests at ManifestUpdateManagerBrowserTestWithFileHandling.
TEST_F(ManifestUpdateDataFetchUtilsTest, TestFileHandlersUnchanged) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_EQ(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateDataFetchUtilsTest, TestSecondFileHandlerAdded) {
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

TEST_F(ManifestUpdateDataFetchUtilsTest, TestFileHandlerChangedName) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->name = u"Comma-Separated Values";

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateDataFetchUtilsTest, TestFileHandlerChangedAction) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  manifest_handlers[0]->action = GURL("/?csvtext");

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateDataFetchUtilsTest, TestFileHandlerExtraAccept) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  std::vector<blink::mojom::ManifestFileHandlerPtr> manifest_handlers =
      GetDefaultManifestFileHandlers();
  std::vector<std::u16string> csv_extensions = {u".csv"};
  manifest_handlers[0]->accept.emplace(u"text/csv", csv_extensions);

  apps::FileHandlers new_handlers =
      CreateFileHandlersFromManifest(manifest_handlers, GURL("http://foo.com"));
  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateDataFetchUtilsTest, TestFileHandlerChangedMimeType) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  old_handlers[0].accept[0].mime_type = "text/csv";
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateDataFetchUtilsTest, TestFileHandlerChangedExtension) {
  apps::FileHandlers old_handlers = GetDefaultAppsFileHandlers();
  old_handlers[0].accept[0].file_extensions.emplace(".csv");
  apps::FileHandlers new_handlers = CreateFileHandlersFromManifest(
      GetDefaultManifestFileHandlers(), GURL("http://foo.com"));

  EXPECT_NE(old_handlers, new_handlers);
}

TEST_F(ManifestUpdateDataFetchUtilsTest, TestImageComparison) {
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

  // Doing a FAST means stop on first error but SLOW means continue to end and
  // give a more detailed error.
  enum PassType { SLOW = 0, FAST = 1 };
  // Which map type the icons should be associated with.
  enum MapType { ANY = 0, MASKED = 1, MONO = 2 };
  // Common icon diff result combinations:
  const IconDiffResult NO_CHANGE = NO_CHANGE_DETECTED;
  const IconDiffResult SIZE_CHANGE = MISMATCHED_IMAGE_SIZES;
  // Result: Both important sizes change.
  const IconDiffResult BOTH_CHANGE =
      static_cast<IconDiffResult>(INSTALL_ICON_CHANGED | LAUNCHER_ICON_CHANGED);
  // Result: All types of sizes change (important and unimportant).
  const IconDiffResult ALL_CHANGE = static_cast<IconDiffResult>(
      INSTALL_ICON_CHANGED | LAUNCHER_ICON_CHANGED | UNIMPORTANT_ICON_CHANGED);

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
    PassType pass_type;
    MapType map_current;
    std::vector<icon> current;
    MapType map_downloaded;
    std::vector<icon> downloaded;
    IconDiffResult expected_diff_result;
  } test_cases[] = {
      // Test: zero icons -> zero icons:
      {FAST, ANY, NoIcons, ANY, NoIcons, NO_CHANGE},
      {SLOW, ANY, NoIcons, ANY, NoIcons, NO_CHANGE},
      // Test: zero icons -> one icon (unimportant size) via 'any' map:
      {FAST, ANY, NoIcons, ANY, Icon1, SIZE_CHANGE},
      {SLOW, ANY, NoIcons, ANY, Icon1, SIZE_CHANGE},
      // Test: single icon -> zero icons:
      {FAST, ANY, Icon1, ANY, NoIcons, SIZE_CHANGE},
      {SLOW, ANY, Icon1, ANY, NoIcons, SIZE_CHANGE},
      // Test: single icon -> single icon (but size changes).
      {FAST, ANY, Icon1, ANY, Icon2, SIZE_CHANGE},
      {SLOW, ANY, Icon1, ANY, Icon2, SIZE_CHANGE},
      // Same as above, except across maps ('any' and 'monochrome').
      {FAST, ANY, Icon1, MONO, Icon2, SIZE_CHANGE},
      {SLOW, ANY, Icon1, MONO, Icon2, SIZE_CHANGE},
      // Same as above, except across maps ('maskable' and 'monochrome').
      {FAST, MASKED, Icon1, MONO, Icon2, SIZE_CHANGE},
      {SLOW, MASKED, Icon1, MONO, Icon2, SIZE_CHANGE},
      // Test: single icon (unimportant size) changes color.
      {FAST, ANY, Icon1, ANY, Icon1Red, ONE_OR_MORE_ICONS_CHANGED},
      {SLOW, ANY, Icon1, ANY, Icon1Red, UNIMPORTANT_ICON_CHANGED},
      // Test: launcher icon changes color.
      {FAST, ANY, Launcher, ANY, LauncherBlue, ONE_OR_MORE_ICONS_CHANGED},
      {SLOW, ANY, Launcher, ANY, LauncherBlue, LAUNCHER_ICON_CHANGED},
      // Test: install icon changes color.
      {FAST, ANY, InstallIcon, ANY, InstallIconCyan, ONE_OR_MORE_ICONS_CHANGED},
      {SLOW, ANY, InstallIcon, ANY, InstallIconCyan, INSTALL_ICON_CHANGED},
      // Test: both Launcher and Install icon changes color.
      {FAST, ANY, BothBefore, ANY, BothAfter, ONE_OR_MORE_ICONS_CHANGED},
      {SLOW, ANY, BothBefore, ANY, BothAfter, BOTH_CHANGE},
      // Test: all types (Launcher, Install and unimportant icon) change color.
      {FAST, ANY, AllBefore, ANY, AllAfter, ONE_OR_MORE_ICONS_CHANGED},
      {SLOW, ANY, AllBefore, ANY, AllAfter, ALL_CHANGE},
  };

  int i = 1;
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE("Test no: " + base::NumberToString(i++) + " expect: " +
                 DiffResultsToString(test_case.expected_diff_result));
    IconBitmaps on_disk;
    for (const auto& current_icon : test_case.current) {
      std::map<SquareSizePx, SkBitmap>* map;
      switch (test_case.map_current) {
        case ANY:
          map = &on_disk.any;
          break;
        case MASKED:
          map = &on_disk.maskable;
          break;
        case MONO:
          map = &on_disk.monochrome;
          break;
      }
      AddGeneratedIcon(map, current_icon.icon_size, current_icon.icon_color);
    }
    IconBitmaps downloaded;
    for (const auto& current_icon : test_case.downloaded) {
      std::map<SquareSizePx, SkBitmap>* map;
      switch (test_case.map_downloaded) {
        case ANY:
          map = &downloaded.any;
          break;
        case MASKED:
          map = &downloaded.maskable;
          break;
        case MONO:
          map = &downloaded.monochrome;
          break;
      }
      AddGeneratedIcon(map, current_icon.icon_size, current_icon.icon_color);
    }

    IconDiff diff = HaveIconBitmapsChanged(
        on_disk, downloaded, GenerateIconInfosFrom(on_disk),
        GenerateIconInfosFrom(downloaded), test_case.pass_type == FAST);
    EXPECT_STREQ(DiffResultsToString(test_case.expected_diff_result).c_str(),
                 DiffResultsToString(diff.diff_results).c_str());

    if ((test_case.expected_diff_result & INSTALL_ICON_CHANGED) != 0) {
      EXPECT_TRUE(diff.requires_app_identity_check());
      ASSERT_FALSE(diff.before.drawsNothing());
      ASSERT_FALSE(diff.after.drawsNothing());
      EXPECT_EQ(starting_install_icon_color, diff.before.getColor(0, 0));
      EXPECT_EQ(ending_install_icon_color, diff.after.getColor(0, 0));
    } else if ((test_case.expected_diff_result & LAUNCHER_ICON_CHANGED) != 0) {
      EXPECT_TRUE(diff.requires_app_identity_check());
      ASSERT_FALSE(diff.before.drawsNothing());
      ASSERT_FALSE(diff.after.drawsNothing());
      EXPECT_EQ(starting_launcher_icon_color, diff.before.getColor(0, 0));
      EXPECT_EQ(ending_launcher_icon_color, diff.after.getColor(0, 0));
    } else {
      EXPECT_FALSE(diff.requires_app_identity_check());
      EXPECT_TRUE(diff.before.drawsNothing());
      EXPECT_TRUE(diff.after.drawsNothing());
    }
  }
}

class ManifestUpdateDataFetchCommandTest : public WebAppTest {
 public:
  ManifestUpdateDataFetchCommandTest()
      : update_dialog_scope_(SetIdentityUpdateDialogActionForTesting(
            AppIdentityUpdate::kAllowed)) {}
  ManifestUpdateDataFetchCommandTest(
      const ManifestUpdateDataFetchCommandTest&) = delete;
  ManifestUpdateDataFetchCommandTest& operator=(
      const ManifestUpdateDataFetchCommandTest&) = delete;
  ~ManifestUpdateDataFetchCommandTest() override = default;

  struct ManifestUpdateDataFetchResult {
    absl::optional<ManifestUpdateResult> update_result;
    absl::optional<WebAppInstallInfo> install_info;
    bool app_identity_update_allowed;
  };

  void SetUp() override {
    WebAppTest::SetUp();
    test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

 protected:
  ManifestUpdateDataFetchResult RunCommandAndGetResult(
      const GURL& url,
      const AppId& app_id,
      std::unique_ptr<FakeDataRetriever> data_retriever) {
    base::RunLoop loop;
    ManifestUpdateDataFetchResult output_result;
    provider().command_manager().ScheduleCommand(
        std::make_unique<ManifestUpdateDataFetchCommand>(
            url, app_id, web_contents()->GetWeakPtr(),
            base::BindLambdaForTesting(
                [&](absl::optional<ManifestUpdateResult> result,
                    absl::optional<WebAppInstallInfo> install_info,
                    bool app_identity_allowed) {
                  output_result.update_result = result;
                  output_result.install_info = std::move(install_info);
                  output_result.app_identity_update_allowed =
                      app_identity_allowed;
                  loop.Quit();
                }),
            std::move(data_retriever)));
    loop.Run();
    return output_result;
  }

  AppId InstallAppFromInfo(std::unique_ptr<WebAppInstallInfo> info) {
    return test::InstallWebApp(profile(), std::move(info));
  }

  std::unique_ptr<FakeDataRetriever> GetFakeDataRetriever(
      bool is_installable,
      const WebAppInstallInfo& info) {
    auto data_retriever = std::make_unique<FakeDataRetriever>();
    data_retriever->SetManifest(GetManifestFromInfo(info), is_installable);
    return data_retriever;
  }

  blink::mojom::ManifestPtr GetManifestFromInfo(const WebAppInstallInfo& info) {
    auto manifest = blink::mojom::Manifest::New();
    manifest->start_url = info.start_url;
    manifest->scope = info.scope;
    manifest->display = info.display_mode;
    manifest->name = info.title;
    return manifest;
  }

  WebAppProvider& provider() { return *WebAppProvider::GetForTest(profile()); }
  GURL app_url() { return app_url_; }

 private:
  const GURL app_url_{"http://www.foo.bar/web_apps/basic.html"};
  base::AutoReset<absl::optional<AppIdentityUpdate>> update_dialog_scope_;
};

TEST_F(ManifestUpdateDataFetchCommandTest, VerifySuccessfulNameUpdate) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify name changes are properly propagated.
  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kStandalone;
  new_info.title = u"New Name";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_FALSE(result.update_result.has_value());
  EXPECT_EQ(result.install_info.value().title, u"New Name");
  EXPECT_TRUE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, VerifySuccessfulScopeUpdate) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify scope changes are properly propagated.
  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = GURL("https://foo.bar.com/new_scope");
  new_info.display_mode = DisplayMode::kStandalone;
  new_info.title = u"Foo App";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_FALSE(result.update_result.has_value());
  EXPECT_EQ(result.install_info.value().scope,
            GURL("https://foo.bar.com/new_scope"));
  EXPECT_FALSE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, VerifySuccessfulDisplayModeUpdate) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify display mode changes are properly propagated.
  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kMinimalUi;
  new_info.title = u"Foo App";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_FALSE(result.update_result.has_value());
  EXPECT_EQ(result.install_info.value().display_mode, DisplayMode::kMinimalUi);
  EXPECT_FALSE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, MultiDataUpdate) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // Verify display mode changes are properly propagated.
  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = GURL("https://foo.bar.com/new_scope/");
  new_info.display_mode = DisplayMode::kMinimalUi;
  new_info.title = u"Foo App 2";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_FALSE(result.update_result.has_value());
  EXPECT_EQ(result.install_info.value().display_mode, DisplayMode::kMinimalUi);
  EXPECT_EQ(result.install_info.value().scope,
            GURL("https://foo.bar.com/new_scope/"));
  EXPECT_EQ(result.install_info.value().title, u"Foo App 2");
  EXPECT_TRUE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, NoAppUpdateNeeded) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // No fields are changed, so no updates should be needed.
  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kStandalone;
  new_info.title = u"Foo App";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_TRUE(result.update_result.has_value());
  EXPECT_EQ(result.update_result.value(), ManifestUpdateResult::kAppUpToDate);
  EXPECT_FALSE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, AppNotEligible) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // start_url changing should not move ahead with a manifest update.
  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kMinimalUi;
  new_info.title = u"Foo App";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/false, new_info));

  EXPECT_TRUE(result.update_result.has_value());
  EXPECT_EQ(result.update_result.value(),
            ManifestUpdateResult::kAppNotEligible);
  EXPECT_FALSE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, AppIdMismatch) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  // start_url changing should not move ahead with a manifest update as the
  // generated app_id is different.
  WebAppInstallInfo new_info;
  new_info.start_url = GURL("https://foo.bar.com/new_app/");
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kMinimalUi;
  new_info.title = u"Foo App";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_TRUE(result.update_result.has_value());
  EXPECT_EQ(result.update_result.value(), ManifestUpdateResult::kAppIdMismatch);
  EXPECT_FALSE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, IconReadFromDiskFailed) {
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kMinimalUi;
  new_info.title = u"Foo App 2";

  base::RunLoop run_loop;
  provider().icon_manager().DeleteData(
      app_id, base::BindLambdaForTesting([&](bool success) {
        EXPECT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_TRUE(result.update_result.has_value());
  EXPECT_EQ(result.update_result.value(),
            ManifestUpdateResult::kIconReadFromDiskFailed);
  EXPECT_FALSE(result.app_identity_update_allowed);
}

TEST_F(ManifestUpdateDataFetchCommandTest, DoNotAcceptAppUpdateDialog) {
  // Ensure we do not accept the app identity dialog for testing.
  base::AutoReset<absl::optional<AppIdentityUpdate>> test_scope =
      SetIdentityUpdateDialogActionForTesting(AppIdentityUpdate::kSkipped);
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->start_url = app_url();
  install_info->scope = app_url().GetWithoutFilename();
  install_info->display_mode = DisplayMode::kStandalone;
  install_info->title = u"Foo App";
  AppId app_id = InstallAppFromInfo(std::move(install_info));

  WebAppInstallInfo new_info;
  new_info.start_url = app_url();
  new_info.scope = app_url().GetWithoutFilename();
  new_info.display_mode = DisplayMode::kStandalone;
  new_info.title = u"Foo App 2";

  ManifestUpdateDataFetchResult result = RunCommandAndGetResult(
      app_url(), app_id,
      GetFakeDataRetriever(/*is_installable=*/true, new_info));

  EXPECT_TRUE(result.update_result.has_value());
  EXPECT_EQ(result.update_result.value(), ManifestUpdateResult::kAppUpToDate);
  EXPECT_FALSE(result.app_identity_update_allowed);
}

}  // namespace web_app
