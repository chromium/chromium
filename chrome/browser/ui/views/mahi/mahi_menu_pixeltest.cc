// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/views/mahi/mahi_condensed_menu_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/widget/widget.h"

namespace chromeos::mahi {

namespace {

constexpr char kScreenshotPrefix[] = "mahi.MahiMenuPixelBrowserTest";
constexpr gfx::Rect kContextMenuRectNarrow = {100, 100, 100, 200};
constexpr gfx::Rect kContextMenuRectWide = {100, 100, 300, 200};

using PixelTestParam = std::tuple<bool, bool, bool>;

bool IsDarkMode(const PixelTestParam& pixel_test_param) {
  return std::get<0>(pixel_test_param);
}

bool IsRtl(const PixelTestParam& pixel_test_param) {
  return std::get<1>(pixel_test_param);
}

bool IsNarrowLayout(const PixelTestParam& pixel_test_param) {
  return std::get<2>(pixel_test_param);
}

std::string GetDarkModeParamValue(const PixelTestParam& pixel_test_param) {
  return IsDarkMode(pixel_test_param) ? "Dark" : "Light";
}

std::string GetRtlParamValue(const PixelTestParam& pixel_test_param) {
  return IsRtl(pixel_test_param) ? "Rtl" : "Ltr";
}

std::string GetNarrowLayoutParamValue(const PixelTestParam& pixel_test_param) {
  return IsNarrowLayout(pixel_test_param) ? "Narrow" : "Wide";
}

std::string GetParamName(const PixelTestParam& param,
                         std::string_view separator) {
  std::vector<std::string> param_names;
  param_names.push_back(GetDarkModeParamValue(param));
  param_names.push_back(GetRtlParamValue(param));
  param_names.push_back(GetNarrowLayoutParamValue(param));
  return base::JoinString(param_names, separator);
}

std::string GenerateParamName(
    const testing::TestParamInfo<PixelTestParam>& test_param_info) {
  return GetParamName(test_param_info.param, /*separator=*/"");
}

std::string GetScreenshotName(const std::string& test_name,
                              const PixelTestParam& param) {
  return test_name + "." + GetParamName(param, /*separator=*/".");
}

// To run a pixel test locally:
//
// browser_tests --gtest_filter=*MahiMenuPixelBrowserTest.*
//   --enable-pixel-output-in-tests
//   --browser-ui-tests-verify-pixels
//   --skia-gold-local-png-write-directory=/tmp/qa_pixel_test
class MahiMenuPixelBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<PixelTestParam> {
 public:
  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (IsRtl(GetParam())) {
      command_line->AppendSwitchASCII(switches::kForceUIDirection,
                                      switches::kForceDirectionRTL);
    }

    InProcessBrowserTest::SetUpCommandLine(command_line);

    if (!command_line->HasSwitch(switches::kVerifyPixels)) {
      GTEST_SKIP() << "A pixel test requires kVerifyPixels flag.";
    }

    pixel_diff_.emplace(kScreenshotPrefix);
  }

  void SetUpOnMainThread() override {
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(
        IsDarkMode(GetParam()));

    InProcessBrowserTest::SetUpOnMainThread();
  }

 protected:
  gfx::Rect GetContextMenuRect() {
    return IsNarrowLayout(GetParam()) ? kContextMenuRectNarrow
                                      : kContextMenuRectWide;
  }

  std::optional<views::ViewSkiaGoldPixelDiff> pixel_diff_;
};

INSTANTIATE_TEST_SUITE_P(PixelTest,
                         MahiMenuPixelBrowserTest,
                         testing::Combine(/*IsDarkMode=*/testing::Bool(),
                                          /*IsRtl=*/testing::Bool(),
                                          /*IsNarrowLayout=*/testing::Bool()),
                         &GenerateParamName);

}  // namespace

IN_PROC_BROWSER_TEST_P(MahiMenuPixelBrowserTest, CondensedMenu) {
  ReadWriteCardsUiController ui_controller;

  ui_controller.SetMahiUi(std::make_unique<MahiCondensedMenuView>());
  ui_controller.SetContextMenuBounds(GetContextMenuRect());

  views::Widget* widget = ui_controller.widget_for_test();
  ASSERT_TRUE(widget);

  EXPECT_TRUE(pixel_diff_->CompareViewScreenshot(
      GetScreenshotName("CondensedMenu", GetParam()),
      widget->GetContentsView()));
}

}  // namespace chromeos::mahi
