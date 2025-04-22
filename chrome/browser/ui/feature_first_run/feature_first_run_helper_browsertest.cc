// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/feature_first_run/feature_first_run_helper.h"

#include <memory>

#include "base/test/mock_callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/controls/rich_controls_container_view.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace feature_first_run {

namespace {

views::Widget* ShowDialog(content::WebContents* web_contents,
                          base::OnceClosure accept_callback = base::DoNothing(),
                          base::OnceClosure cancel_callback = base::DoNothing(),
                          std::u16string title = std::u16string(),
                          ui::ImageModel banner = ui::ImageModel(),
                          ui::ImageModel dark_mode_banner = ui::ImageModel(),
                          std::unique_ptr<views::View> content_view =
                              std::make_unique<views::View>()) {
  return ShowFeatureFirstRunDialog(
      std::move(title), std::move(banner), std::move(dark_mode_banner),
      std::move(content_view), std::move(accept_callback),
      std::move(cancel_callback), web_contents);
}

}  // namespace

using FeatureFirstRunDialogHelperBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(FeatureFirstRunDialogHelperBrowserTest,
                       DialogConstructedFromParams) {
  const std::u16string title = u"Test Title";
  auto content_view = std::make_unique<views::View>();
  auto* expected_content_view = content_view.get();
  auto banner = ui::ImageModel::FromResourceId(IDR_SAVE_PASSPORT);
  auto dark_mode_banner =
      ui::ImageModel::FromResourceId(IDR_SAVE_PASSPORT_DARK);

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* dialog_widget =
      ShowDialog(web_contents, base::DoNothing(), base::DoNothing(), title,
                 banner, dark_mode_banner, std::move(content_view));
  auto* dialog_widget_delegate = dialog_widget->widget_delegate();

  EXPECT_TRUE(dialog_widget->IsVisible());
  EXPECT_EQ(title, dialog_widget_delegate->GetWindowTitle());

  auto* bubble_frame_view = static_cast<views::BubbleFrameView*>(
      dialog_widget->non_client_view()->frame_view());
  auto* banner_view = static_cast<views::ImageView*>(
      bubble_frame_view->GetHeaderViewForTesting());

  // Verify light mode banner.
  auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser()->profile());
  theme_service->SetBrowserColorScheme(
      ThemeService::BrowserColorScheme::kLight);
  EXPECT_EQ(banner.GetImage().ToSkBitmap(), banner_view->GetImage().bitmap());

  // Verify dark mode banner.
  theme_service->SetBrowserColorScheme(ThemeService::BrowserColorScheme::kDark);
  EXPECT_EQ(dark_mode_banner.GetImage().ToSkBitmap(),
            banner_view->GetImage().bitmap());

  auto* actual_content_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kFeatureFirstRunDialogContentViewElementId,
          browser()->window()->GetElementContext());
  EXPECT_EQ(expected_content_view, actual_content_view);
}

IN_PROC_BROWSER_TEST_F(FeatureFirstRunDialogHelperBrowserTest,
                       OnDialogAccepted) {
  base::MockOnceClosure accept_callback;

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* dialog_widget = ShowDialog(web_contents, accept_callback.Get());

  EXPECT_CALL(accept_callback, Run);
  dialog_widget->widget_delegate()->AsDialogDelegate()->Accept();
}

IN_PROC_BROWSER_TEST_F(FeatureFirstRunDialogHelperBrowserTest,
                       OnDialogCancelled) {
  base::MockOnceClosure cancel_callback;

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* dialog_widget =
      ShowDialog(web_contents, base::DoNothing(), cancel_callback.Get());

  EXPECT_CALL(cancel_callback, Run);
  dialog_widget->widget_delegate()->AsDialogDelegate()->Cancel();
}

IN_PROC_BROWSER_TEST_F(FeatureFirstRunDialogHelperBrowserTest,
                       InfoBoxConstructedFromParams) {
  const std::u16string title = u"Test Title";
  // TODO(crbug.com/409520456): Test that the description is passed correctly.
  // RichControlsContainerView currently doesn't expose secondary labels or
  // store a pointer to them.
  const std::u16string description = u"Test Description";
  const gfx::VectorIcon& icon = kTextAnalysisIcon;
  const int radius = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_FEATURE_FIRST_RUN_INFO_BOX_ROUNDED_BORDER_RADIUS);

  auto info_box_start =
      CreateInfoBoxContainer(title, description, icon, InfoBoxPosition::kStart);

  EXPECT_EQ(title, info_box_start->GetTitleForTesting());
  EXPECT_EQ(&icon,
            info_box_start->GetIconForTesting().GetVectorIcon().vector_icon());
  EXPECT_EQ(gfx::RoundedCornersF(radius, radius, 0, 0),
            info_box_start->GetBackground()->GetRoundedCornerRadii());

  auto info_box_middle = CreateInfoBoxContainer(title, description, icon,
                                                InfoBoxPosition::kMiddle);
  EXPECT_FALSE(info_box_middle->GetBackground()->GetRoundedCornerRadii());

  auto info_box_end =
      CreateInfoBoxContainer(title, description, icon, InfoBoxPosition::kEnd);
  EXPECT_EQ(gfx::RoundedCornersF(0, 0, radius, radius),
            info_box_end->GetBackground()->GetRoundedCornerRadii());
}

}  // namespace feature_first_run
