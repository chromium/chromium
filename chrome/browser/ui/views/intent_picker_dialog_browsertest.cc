// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "chrome/browser/ui/views/location_bar/intent_chip_button.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/models/image_model.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"
#include "ui/views/widget/widget_utils.h"
#include "url/gurl.h"

class IntentPickerDialogTest : public DialogBrowserTest {
 public:
  IntentPickerDialogTest() = default;
  IntentPickerDialogTest(const IntentPickerDialogTest&) = delete;
  IntentPickerDialogTest& operator=(const IntentPickerDialogTest&) = delete;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    std::vector<apps::IntentPickerAppInfo> app_info;
    const auto add_entry = [&app_info](const std::string& str) {
      auto icon_size = IntentPickerTabHelper::GetIntentPickerBubbleIconSize();
      app_info.emplace_back(
          apps::PickerEntryType::kUnknown,
          ui::ImageModel::FromImage(
              gfx::Image::CreateFrom1xBitmap(favicon::GenerateMonogramFavicon(
                  GURL("https://" + str + ".com"), icon_size, icon_size))),
          "Launch name " + str, "Display name " + str);
    };
    add_entry("a");
    add_entry("b");
    add_entry("c");
    add_entry("d");
    IntentPickerBubbleView::ShowBubble(
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView(),
        GetAnchorButton(), IntentPickerBubbleView::BubbleType::kLinkCapturing,
        browser()->tab_strip_model()->GetActiveWebContents(),
        std::move(app_info), true, true,
        url::Origin::Create(GURL("https://c.com")), base::DoNothing());
  }

 private:
  virtual views::Button* GetAnchorButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetPageActionIconView(PageActionIconType::kIntentPicker);
  }
};

#if BUILDFLAG(IS_MAC)
// Flaky on Mac. See https://crbug.com/1330302.
#define MAYBE_InvokeUi_default DISABLED_InvokeUi_default
#else
#define MAYBE_InvokeUi_default InvokeUi_default
#endif
IN_PROC_BROWSER_TEST_F(IntentPickerDialogTest, MAYBE_InvokeUi_default) {
  set_baseline("5428271");
  ShowAndVerifyUi();
}

class IntentPickerDialogGridViewTest : public IntentPickerDialogTest {
 public:
  IntentPickerDialogGridViewTest() {
    feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(), {});
  }

  void ShowUi(const std::string& name) override {
    IntentPickerDialogTest::ShowUi(name);

    // Click the first item in the list so we can verify the selection state.
    auto* bubble = IntentPickerBubbleView::intent_picker_bubble();
    auto event_generator =
        ui::test::EventGenerator(views::GetRootWindow(bubble->GetWidget()));
    auto* button =
        bubble->GetViewByID(IntentPickerBubbleView::ViewId::kItemContainer)
            ->children()[0]
            .get();
    event_generator.MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
    event_generator.ClickLeftButton();
  }

 private:
  views::Button* GetAnchorButton() override {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar_button_provider()
        ->GetIntentChipButton();
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(IntentPickerDialogGridViewTest, InvokeUi_default) {
  set_baseline("5428271");
  ShowAndVerifyUi();
}
