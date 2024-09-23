// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_app_utils.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/web_applications/test/test_file_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/touchscreen_device.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/components/arc/arc_util.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/startup/browser_init_params.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace web_app {

namespace {

ui::TouchscreenDevice CreateTouchDevice(ui::InputDeviceType type,
                                        bool stylus_support) {
  ui::TouchscreenDevice touch_device = ui::TouchscreenDevice();
  touch_device.type = type;
  touch_device.has_stylus = stylus_support;
  return touch_device;
}

}  // namespace

class PreinstalledWebAppUtilsTest : public testing::Test {
 public:
  PreinstalledWebAppUtilsTest() = default;
  ~PreinstalledWebAppUtilsTest() override = default;

  // testing::Test:
  void SetUp() override {
    testing::Test::SetUp();

    base::FilePath source_root_dir;
    CHECK(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root_dir));
    file_utils_ = TestFileUtils::Create({
        {base::FilePath(FILE_PATH_LITERAL("test_dir/icon.png")),
         source_root_dir.AppendASCII("chrome/test/data/web_apps/blue-192.png")},
        {base::FilePath(FILE_PATH_LITERAL("test_dir/basic.html")),
         source_root_dir.AppendASCII("chrome/test/data/web_apps/basic.html")},
    });
  }

  std::optional<ExternalInstallOptions> ParseConfig(
      const char* app_config_string) {
    std::optional<base::Value> app_config =
        base::JSONReader::Read(app_config_string);
    DCHECK(app_config);
    auto file_utils = base::MakeRefCounted<FileUtilsWrapper>();
    OptionsOrError result =
        ::web_app::ParseConfig(*file_utils, /*dir=*/base::FilePath(),
                               /*file=*/base::FilePath(), app_config.value());
    if (ExternalInstallOptions* options =
            absl::get_if<ExternalInstallOptions>(&result)) {
      return std::move(*options);
    }
    return std::nullopt;
  }

  std::optional<WebAppInstallInfoFactory> ParseOfflineManifest(
      const char* offline_manifest_string) {
    std::optional<base::Value> offline_manifest =
        base::JSONReader::Read(offline_manifest_string);
    DCHECK(offline_manifest);
    WebAppInstallInfoFactoryOrError result = ::web_app::ParseOfflineManifest(
        *file_utils_, base::FilePath(FILE_PATH_LITERAL("test_dir")),
        base::FilePath(FILE_PATH_LITERAL("test_dir/test.json")),
        *offline_manifest);
    if (WebAppInstallInfoFactory* factory =
            absl::get_if<WebAppInstallInfoFactory>(&result)) {
      return std::move(*factory);
    }
    return std::nullopt;
  }

 protected:
  scoped_refptr<TestFileUtils> file_utils_;
};

// ParseConfig() is also tested by PreinstalledWebAppManagerTest.

#if BUILDFLAG(IS_CHROMEOS)

namespace {

std::string BoolParamToString(
    const ::testing::TestParamInfo<bool>& bool_param) {
  return bool_param.param ? "true" : "false";
}

using IsTablet = bool;
using IsArcSupported = bool;

}  // namespace

class PreinstalledWebAppUtilsTabletTest
    : public PreinstalledWebAppUtilsTest,
      public ::testing::WithParamInterface<IsTablet> {
 public:
  PreinstalledWebAppUtilsTabletTest() {
    if (GetParam()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          ash::switches::kEnableTabletFormFactor);
#else
      auto init_params = crosapi::mojom::BrowserInitParams::New();
      init_params->device_properties = crosapi::mojom::DeviceProperties::New();
      init_params->device_properties->is_tablet_form_factor = true;
      chromeos::BrowserInitParams::SetInitParamsForTests(
          std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }
  }
  ~PreinstalledWebAppUtilsTabletTest() override = default;

  bool is_tablet() const { return GetParam(); }
};

TEST_P(PreinstalledWebAppUtilsTabletTest, DisableIfTabletFormFactor) {
  std::optional<ExternalInstallOptions> disable_true_options = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "disable_if_tablet_form_factor": true,
      "user_type": ["test"]
    }
  )");
  EXPECT_TRUE(disable_true_options->disable_if_tablet_form_factor);

  std::optional<ExternalInstallOptions> disable_false_options = ParseConfig(R"(
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
                         PreinstalledWebAppUtilsTabletTest,
                         ::testing::Values(true, false),
                         BoolParamToString);

class PreinstalledWebAppUtilsArcTest
    : public PreinstalledWebAppUtilsTest,
      public ::testing::WithParamInterface<IsArcSupported> {
 public:
  PreinstalledWebAppUtilsArcTest() {
    if (GetParam()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          ash::switches::kArcAvailability, "officially-supported");
#else
      auto init_params = crosapi::mojom::BrowserInitParams::New();
      init_params->device_properties = crosapi::mojom::DeviceProperties::New();
      init_params->device_properties->is_arc_available = true;
      chromeos::BrowserInitParams::SetInitParamsForTests(
          std::move(init_params));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    }
  }
  ~PreinstalledWebAppUtilsArcTest() override = default;

  bool is_arc_supported() const { return GetParam(); }
};

TEST_P(PreinstalledWebAppUtilsArcTest, DisableIfArcSupported) {
  std::optional<ExternalInstallOptions> disable_true_options = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "disable_if_arc_supported": true,
      "user_type": ["test"]
    }
  )");
  EXPECT_TRUE(disable_true_options->disable_if_arc_supported);

  std::optional<ExternalInstallOptions> disable_false_options = ParseConfig(R"(
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
                         PreinstalledWebAppUtilsArcTest,
                         ::testing::Values(true, false),
                         BoolParamToString);

#endif  // BUILDFLAG(IS_CHROMEOS)

// TODO(crbug.com/40145619): Loading icon.png is flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OfflineManifestValid DISABLED_OfflineManifestValid
#else
#define MAYBE_OfflineManifestValid OfflineManifestValid
#endif
TEST_F(PreinstalledWebAppUtilsTest, MAYBE_OfflineManifestValid) {
  std::unique_ptr<WebAppInstallInfo> app_info = ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_any_pngs": ["icon.png"],
      "icon_maskable_pngs": ["icon.png"],
      "theme_color_argb_hex": "AABBCCDD"
    }
  )")
                                                    .value()
                                                    .Run();
  EXPECT_TRUE(app_info);
  EXPECT_EQ(app_info->title, u"Test App");
  EXPECT_EQ(app_info->start_url(), GURL("https://test.org/start.html"));
  EXPECT_EQ(app_info->scope, GURL("https://test.org/"));
  EXPECT_EQ(app_info->display_mode, DisplayMode::kStandalone);
  EXPECT_EQ(app_info->icon_bitmaps.any.size(), 1u);
  EXPECT_EQ(app_info->icon_bitmaps.any.at(192).getColor(0, 0), SK_ColorBLUE);
  EXPECT_EQ(app_info->icon_bitmaps.maskable.size(), 1u);
  EXPECT_EQ(app_info->icon_bitmaps.maskable.at(192).getColor(0, 0),
            SK_ColorBLUE);
  EXPECT_EQ(app_info->theme_color, SkColorSetARGB(0xFF, 0xBB, 0xCC, 0xDD));
}

TEST_F(PreinstalledWebAppUtilsTest, OfflineManifestName) {
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

TEST_F(PreinstalledWebAppUtilsTest, OfflineManifestStartUrl) {
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

TEST_F(PreinstalledWebAppUtilsTest, OfflineManifestScope) {
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

// TODO(crbug.com/40145619): Loading icon.png is flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OfflineManifestDisplay DISABLED_OfflineManifestDisplay
#else
#define MAYBE_OfflineManifestDisplay OfflineManifestDisplay
#endif
TEST_F(PreinstalledWebAppUtilsTest, MAYBE_OfflineManifestDisplay) {
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

TEST_F(PreinstalledWebAppUtilsTest, OfflineManifestIconAnyPngs) {
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

TEST_F(PreinstalledWebAppUtilsTest, OfflineManifestIconMaskablePngs) {
  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone"
    }
  )")) << "icon_any_pngs or icon_maskable_pngs is required";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_maskable_pngs": "icon.png"
    }
  )")) << "icon_maskable_pngs is valid";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_maskable_pngs": ["does-not-exist.png"]
    }
  )")) << "icon_maskable_pngs exists";

  EXPECT_FALSE(ParseOfflineManifest(R"(
    {
      "name": "Test App",
      "start_url": "https://test.org/start.html",
      "scope": "https://test.org/",
      "display": "standalone",
      "icon_maskable_pngs": ["basic.html"]
    }
  )")) << "icon_maskable_pngs is a PNG";
}

TEST_F(PreinstalledWebAppUtilsTest, OfflineManifestThemeColorArgbHex) {
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

TEST_F(PreinstalledWebAppUtilsTest, ForceReinstallForMilestone) {
  std::optional<ExternalInstallOptions> non_number = ParseConfig(R"(
    {
      "app_url": "https://test.org",
      "launch_container": "window",
      "force_reinstall_for_milestone": "error",
      "user_type": ["test"]
    }
  )");
  EXPECT_FALSE(non_number.has_value());

  std::optional<ExternalInstallOptions> number = ParseConfig(R"(
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

TEST_F(PreinstalledWebAppUtilsTest, IsReinstallPastMilestoneNeeded) {
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

TEST_F(PreinstalledWebAppUtilsTest, OemInstalled) {
  std::optional<ExternalInstallOptions> non_bool = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "oem_installed": "some string",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(non_bool.has_value());

  std::optional<ExternalInstallOptions> no_oem = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(no_oem->oem_installed);

  std::optional<ExternalInstallOptions> oem_set = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "oem_installed": true,
          "user_type": ["test"]
        }
    )");
  EXPECT_TRUE(oem_set->oem_installed);
}

TEST_F(PreinstalledWebAppUtilsTest,
       DisableIfTouchscreenWithStylusNotSupported) {
  std::optional<ExternalInstallOptions> non_bool = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "disable_if_touchscreen_with_stylus_not_supported": "some string",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(non_bool.has_value());

  std::optional<ExternalInstallOptions> default_setting = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(
      default_setting->disable_if_touchscreen_with_stylus_not_supported);

  std::optional<ExternalInstallOptions> touchscreen_set = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "disable_if_touchscreen_with_stylus_not_supported": true,
          "user_type": ["test"]
        }
    )");
  EXPECT_TRUE(
      touchscreen_set->disable_if_touchscreen_with_stylus_not_supported);
}

TEST_F(PreinstalledWebAppUtilsTest, GateOnFeatureNameOrInstalled) {
  std::optional<ExternalInstallOptions> feature_name_set = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "feature_name_or_installed": "foobar",
          "user_type": ["test"]
        }
    )");
  EXPECT_EQ("foobar", feature_name_set->gate_on_feature_or_installed);

  std::optional<ExternalInstallOptions> no_feature_name = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(no_feature_name->gate_on_feature_or_installed.has_value());

  std::optional<ExternalInstallOptions> non_string_feature = ParseConfig(R"(
        {
          "app_url": "https://www.test.org",
          "launch_container": "window",
          "feature_name_or_installed": true,
          "user_type": ["test"]
        }
    )");
  EXPECT_FALSE(non_string_feature->gate_on_feature_or_installed.has_value());
}

class PreinstalledWebAppUtilsDeviceManagerTest
    : public PreinstalledWebAppUtilsTest {
 public:
  void SetUp() override {
    if (!ui::DeviceDataManager::HasInstance()) {
      GTEST_SKIP() << "No DeviceDataManager available";
    }

    ui::DeviceDataManager::GetInstance()->ResetDeviceListsForTest();
  }
};

TEST_F(PreinstalledWebAppUtilsDeviceManagerTest,
       HasStylusEnabledTouchscreen_Uninitialized) {
  // Do not initialize DeviceDataManager.

  ASSERT_FALSE(DeviceHasStylusEnabledTouchscreen().has_value());
}

TEST_F(PreinstalledWebAppUtilsDeviceManagerTest,
       HasStylusEnabledTouchscreen_NoTouchscreen) {
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({});
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  ASSERT_TRUE(DeviceHasStylusEnabledTouchscreen().has_value());
  ASSERT_FALSE(DeviceHasStylusEnabledTouchscreen().value());
}

TEST_F(PreinstalledWebAppUtilsDeviceManagerTest,
       HasStylusEnabledTouchscreen_NonStylusTouchscreen) {
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({CreateTouchDevice(
      ui::InputDeviceType::INPUT_DEVICE_INTERNAL, /* stylus_support =*/false)});
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  ASSERT_TRUE(DeviceHasStylusEnabledTouchscreen().has_value());
  ASSERT_FALSE(DeviceHasStylusEnabledTouchscreen().value());
}

TEST_F(PreinstalledWebAppUtilsDeviceManagerTest,
       HasStylusEnabledTouchscreen_ExternalStylusTouchscreen) {
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({CreateTouchDevice(
      ui::InputDeviceType::INPUT_DEVICE_USB, /* stylus_support =*/true)});
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  ASSERT_TRUE(DeviceHasStylusEnabledTouchscreen().has_value());
  ASSERT_FALSE(DeviceHasStylusEnabledTouchscreen().value());
}

TEST_F(PreinstalledWebAppUtilsDeviceManagerTest,
       HasStylusEnabledTouchscreen_InternalStylusTouchscreen) {
  ui::DeviceDataManagerTestApi().SetTouchscreenDevices({CreateTouchDevice(
      ui::InputDeviceType::INPUT_DEVICE_INTERNAL, /* stylus_support =*/true)});
  ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

  ASSERT_TRUE(DeviceHasStylusEnabledTouchscreen().has_value());
  ASSERT_TRUE(DeviceHasStylusEnabledTouchscreen().value());
}

}  // namespace web_app
