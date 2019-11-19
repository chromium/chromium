// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/feature_promos/global_media_controls_promo_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "media/base/media_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/native_theme/test_native_theme.h"
#include "ui/views/animation/ink_drop.h"

namespace {

constexpr SkColor kTestHighlightColor = SK_ColorBLUE;

class PromoTestNativeTheme : public ui::TestNativeTheme {
 public:
  PromoTestNativeTheme() = default;
  ~PromoTestNativeTheme() override = default;

  // ui::NativeTheme implementation.
  SkColor GetSystemColor(ColorId color_id,
                         ColorScheme color_scheme) const override {
    if (color_id == kColorId_ProminentButtonColor)
      return kTestHighlightColor;
    return ui::TestNativeTheme::GetSystemColor(color_id, color_scheme);
  }
};

}  // anonymous namespace

class GlobalMediaControlsPromoControllerDialogBrowserTest
    : public DialogBrowserTest {
 public:
  GlobalMediaControlsPromoControllerDialogBrowserTest() = default;

  void SetUp() override {
    // Need to enable GMC before the browser is built so that the toolbar is
    // constructed properly.
    feature_list_.InitAndEnableFeature(media::kGlobalMediaControls);
    DialogBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    GetMediaToolbarButton()->SetNativeThemeForTesting(&test_theme_);
  }

  void ShowUi(const std::string& name) override { ShowPromo(); }

  void ShowPromo() {
    ShowMediaToolbarButton();
    GetMediaToolbarButton()->ShowPromo();
  }

  void DisableBubbleTimeout() {
    promo_controller()->disable_bubble_timeout_for_test();
  }

  bool PromoBubbleVisible() {
    views::View* bubble = promo_controller()->promo_bubble_for_test();
    return bubble && bubble->GetVisible();
  }

  bool PromoHighlightVisible() {
    return GetMediaToolbarButton()->GetInkDrop()->GetTargetInkDropState() ==
               views::InkDropState::ACTIVATED &&
           GetMediaToolbarButton()->GetInkDropBaseColor() ==
               kTestHighlightColor;
  }

  void SimulateMediaDialogOpened() {
    GetMediaToolbarButton()->ButtonPressed(
        GetMediaToolbarButton(),
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), 0, 0));
  }

  void SimulateMediaToolbarButtonDisabledOrHidden() {
    GetMediaToolbarButton()->Disable();
  }

 private:
  MediaToolbarButtonView* GetMediaToolbarButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->media_button();
  }

  // Forces the media toolbar button to appear so we can add the promo to it.
  void ShowMediaToolbarButton() {
    auto* media_button = GetMediaToolbarButton();
    media_button->Show();
    media_button->Enable();
  }

  GlobalMediaControlsPromoController* promo_controller() {
    return GetMediaToolbarButton()->GetPromoControllerForTesting();
  }

  PromoTestNativeTheme test_theme_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       InvokeUi_default) {
  DisableBubbleTimeout();
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       BubbleHidesAfter5Seconds) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner);

  EXPECT_FALSE(PromoBubbleVisible());
  EXPECT_FALSE(PromoHighlightVisible());

  ShowPromo();

  // The promo should hide after 5 seconds.
  EXPECT_TRUE(PromoBubbleVisible());
  EXPECT_TRUE(PromoHighlightVisible());
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(4900));
  EXPECT_TRUE(PromoBubbleVisible());
  EXPECT_TRUE(PromoHighlightVisible());
  task_runner->FastForwardBy(base::TimeDelta::FromMilliseconds(200));
  EXPECT_FALSE(PromoBubbleVisible());
  EXPECT_FALSE(PromoHighlightVisible());
}

#if defined(OS_MACOSX)
// TODO(https://crbug.com/998342): Fix this on Mac along with the
// MediaDialogView tests.
#define MAYBE_BubbleHidesIfTheMediaDialogIsOpened \
  DISABLED_BubbleHidesIfTheMediaDialogIsOpened
#else
#define MAYBE_BubbleHidesIfTheMediaDialogIsOpened \
  BubbleHidesIfTheMediaDialogIsOpened
#endif
IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       MAYBE_BubbleHidesIfTheMediaDialogIsOpened) {
  DisableBubbleTimeout();
  ShowPromo();

  EXPECT_TRUE(PromoBubbleVisible());
  EXPECT_TRUE(PromoHighlightVisible());

  SimulateMediaDialogOpened();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(PromoBubbleVisible());
  EXPECT_FALSE(PromoHighlightVisible());
}

IN_PROC_BROWSER_TEST_F(GlobalMediaControlsPromoControllerDialogBrowserTest,
                       BubbleHidesIfTheMediaToolbarButtonIsDisabledOrHidden) {
  DisableBubbleTimeout();
  ShowPromo();

  EXPECT_TRUE(PromoBubbleVisible());
  EXPECT_TRUE(PromoHighlightVisible());

  SimulateMediaToolbarButtonDisabledOrHidden();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(PromoBubbleVisible());
  EXPECT_FALSE(PromoHighlightVisible());
}
