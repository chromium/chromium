// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/external_web_app_utils.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "components/arc/arc_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace web_app {

class ExternalWebAppUtilsTest : public testing::Test {
 public:
  ExternalWebAppUtilsTest() = default;
  ~ExternalWebAppUtilsTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    base::FilePath source_root_dir;
    CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir));
    file_utils_ = TestFileUtils::Create({
        {base::FilePath(FILE_PATH_LITERAL("test_dir/icon.png")),
         source_root_dir.AppendASCII("chrome/test/data/web_apps/blue-192.png")},
        {base::FilePath(FILE_PATH_LITERAL("test_dir/basic.html")),
         source_root_dir.AppendASCII("chrome/test/data/web_apps/basic.html")},
    });
  }

  base::Optional<ExternalInstallOptions> ParseConfig(
      const char* app_config_string) {
    base::Optional<base::Value> app_config =
        base::JSONReader::Read(app_config_string);
    DCHECK(app_config);
    FileUtilsWrapper file_utils;
    OptionsOrError result =
        ::web_app::ParseConfig(file_utils, /*dir=*/base::FilePath(),
                               /*file=*/base::FilePath(), app_config.value());
    if (ExternalInstallOptions* options =
            absl::get_if<ExternalInstallOptions>(&result)) {
      return std::move(*options);
    }
    return base::nullopt;
  }

  base::Optional<WebApplicationInfoFactory> ParseOfflineManifest(
      const char* offline_manifest_string) {
    base::Optional<base::Value> offline_manifest =
        base::JSONReader::Read(offline_manifest_string);
    DCHECK(offline_manifest);
    WebApplicationInfoFactoryOrError result = ::web_app::ParseOfflineManifest(
        *file_utils_, base::FilePath(FILE_PATH_LITERAL("test_dir")),
        base::FilePath(FILE_PATH_LITERAL("test_dir/test.json")),
        *offline_manifest);
    if (WebApplicationInfoFactory* factory =
            absl::get_if<WebApplicationInfoFactory>(&result)) {
      return std::move(*factory);
    }
    return base::nullopt;
  }

 protected:
  std::unique_ptr<TestFileUtils> file_utils_;
};

// ParseConfig() is also tested by ExternalWebAppManagerTest.

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace {

std::string BoolParamToString(
    const ::testing::TestParamInfo<bool>& bool_param) {
  return bool_param.param ? "true" : "false";
}

using IsTablet = bool;
using IsArcSupported = bool;

}  // namespace

class ExternalWebAppUtilsTabletTest
    : public ExternalWebAppUtilsTest,
      public ::testing::WithParamInterface<IsTablet> {
 public:
  ExternalWebAppUtilsTabletTest() {
    if (GetParam()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          chromeos::switches::kEnableTabletFormFactor);
    }
  }
  ~ExternalWebAppUtilsTabletTest() override = default;

  bool is_tablet() const { return GetParam(); }
};

TEST_P(ExternalWebAppUtilsTabletTest, DisableIfTabletFormFactor) {
  base::Optional<ExternalInstallOptions> disable_true_options = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "disable_if_tablet_form_factor": true,
      "user_type": ["test"]
    }
  )");
  EXPECT_TRUE(disable_true_options->disable_if_tablet_form_factor);

  base::Optional<ExternalInstallOptions> disable_false_options = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "disable_if_tablet_form_factor": false,
      "user_type": ["test"]
    }
  )");
  EXPECT_FALSE(disable_false_options->disable_if_tablet_form_factor);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExternalWebAppUtilsTabletTest,
                         ::testing::Values(true, false),
                         BoolParamToString);

class ExternalWebAppUtilsArcTest
    : public ExternalWebAppUtilsTest,
      public ::testing::WithParamInterface<IsArcSupported> {
 public:
  ExternalWebAppUtilsArcTest() {
    if (GetParam()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          chromeos::switches::kArcAvailability, "officially-supported");
    }
  }
  ~ExternalWebAppUtilsArcTest() override = default;

  bool is_arc_supported() const { return GetParam(); }
};

TEST_P(ExternalWebAppUtilsArcTest, DisableIfArcSupported) {
  base::Optional<ExternalInstallOptions> disable_true_options = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "disable_if_arc_supported": true,
      "user_type": ["test"]
    }
  )");
  EXPECT_TRUE(disable_true_options->disable_if_arc_supported);

  base::Optional<ExternalInstallOptions> disable_false_options = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "disable_if_arc_supported": false,
      "user_type": ["test"]
    }
  )");
  EXPECT_FALSE(disable_false_options->disable_if_arc_supported);
}

INSTANTIATE_TEST_SUITE_P(All,
                         ExternalWebAppUtilsArcTest,
                         ::testing::Values(true, false),
                         BoolParamToString);

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(crbug.com/1119710): Loading icon.png is flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_OfflineManifestValid DISABLED_OfflineManifestValid
#else
#define MAYBE_OfflineManifestValid OfflineManifestValid
#endif
TEST_F(ExternalWebAppUtilsTest, MAYBE_OfflineManifestValid) {
  std::unique_ptr<WebApplicationInfo> app_info = ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"],
      "theme_color_argb_hex": "AABBCCDD"
    }
  )")
                                                     .value()
                                                     .Run();
  EXPECT_TRUE(app_info);
  EXPECT_EQ(app_info->title, u"Test App");
  EXPECT_EQ(app_info->start_url, GURL("https://test.org/start.html"));
  EXPECT_EQ(app_info->scope, GURL("https://test.org/"));
  EXPECT_EQ(app_info->display_mode, DisplayMode::kStandalone);
  EXPECT_EQ(app_info->icon_bitmaps.any.size(), 1u);
  EXPECT_EQ(app_info->icon_bitmaps.any.at(192).getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(app_info->theme_color, SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
}

TEST_F(ExternalWebAppUtilsTest, OfflineManifestName) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "name is required";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": 400,
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "name is string";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "name is non-empty";
}

TEST_F(ExternalWebAppUtilsTest, OfflineManifestStartUrl) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "start_url is required";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "not a url",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "start_url is valid";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/inner/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "start_url is within scope";
}

TEST_F(ExternalWebAppUtilsTest, OfflineManifestScope) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "scope is required";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "not a url",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "start_url is valid";
}

// TODO(crbug.com/1119710): Loading icon.png is flaky on Windows.
#if defined(OS_WIN)
#define MAYBE_OfflineManifestDisplay DISABLED_OfflineManifestDisplay
#else
#define MAYBE_OfflineManifestDisplay OfflineManifestDisplay
#endif
TEST_F(ExternalWebAppUtilsTest, MAYBE_OfflineManifestDisplay) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "display is required";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "tab",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "display is valid";

  EXPECT_TRUE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "display can be standalone";
  EXPECT_TRUE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "browser",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "display can be browser";
  EXPECT_TRUE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "minimal-ui",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "display can be minimal-ui";
  EXPECT_TRUE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "fullscreen",
      "icon_any_pngs": ["icon.png"]
    }
  )")) << "display can be fullscreen";
}

TEST_F(ExternalWebAppUtilsTest, OfflineManifestIconAnyPngs) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone"
    }
  )")) << "icon_any_pngs is required";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": "icon.png"
    }
  )")) << "icon_any_pngs is valid";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": [{
        "src": "https://test.org/icon.png",
        "sizes": "144x144",
        "type": "image/png"
      }]
    }
  )")) << "icon_any_pngs is valid";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["does-not-exist.png"]
    }
  )")) << "icon_any_pngs exists";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["basic.html"]
    }
  )")) << "icon_any_pngs is a PNG";
}

TEST_F(ExternalWebAppUtilsTest, OfflineManifestThemeColorArgbHex) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"],
      "theme_color_argb_hex": 12345
    }
  )")) << "theme_color_argb_hex is valid";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"],
      "theme_color_argb_hex": "blue"
    }
  )")) << "theme_color_argb_hex is valid";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"],
      "theme_color_argb_hex": "#ff0000"
    }
  )")) << "theme_color_argb_hex is valid";
}

TEST_F(ExternalWebAppUtilsTest, ForceReinstallForMilestone) {
  base::Optional<ExternalInstallOptions> non_number = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "force_reinstall_for_milestone": "error",
      "user_type": ["test"]
    }
  )");
  EXPECT_FALSE(non_number.has_value());

  base::Optional<ExternalInstallOptions> number = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "force_reinstall_for_milestone": 89,
      "user_type": ["test"]
    }
  )");
  EXPECT_TRUE(number.has_value());
  EXPECT_EQ(89, number->force_reinstall_for_milestone);
}

TEST_F(ExternalWebAppUtilsTest, IsReinstallPastMilestoneNeeded) {
  // Arguments: last_preinstall_synchronize_milestone, current_milestone,
  // force_reinstall_for_milestone.
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("87", "87", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("87", "88", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("88", "88", 89));
  EXPECT_TRUE(IsReinstallPastMilestoneNeeded("88", "89", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("89", "89", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("89", "90", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("90", "90", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("90", "91", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("91", "91", 89));

  // Long jumps:
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("80", "85", 89));
  EXPECT_TRUE(IsReinstallPastMilestoneNeeded("80", "100", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("90", "95", 89));

  // Wrong input:
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("error", "90", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("88", "error", 89));
  EXPECT_FALSE(IsReinstallPastMilestoneNeeded("error", "error", 0));
}

TEST_F(ExternalWebAppUtilsTest, OemInstalled) {
  base::Optional<ExternalInstallOptions> non_bool = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "oem_installed": "some string",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(non_bool.has_value());

  base::Optional<ExternalInstallOptions> no_oem = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(no_oem->oem_installed);

  base::Optional<ExternalInstallOptions> oem_set = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "oem_installed": true,
          "user_type": ["test"]
        }
    )");
  EXPECT_TRUE(oem_set->oem_installed);
}

}  // namespace web_app
