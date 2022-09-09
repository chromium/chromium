// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/lens/lens_side_panel_view.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_side_panel_helper.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/vector_icons.h"

namespace {

std::unique_ptr<views::WebView> CreateWebView(
    views::View* host,
    content::BrowserContext* browser_context) {
  auto webview = std::make_unique<views::WebView>(browser_context);
  // Set a flex behavior for the WebView to always fill out the extra space in
  // the parent view. In the minimum case, it will scale down to 0.
  webview->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  // Set background of webview to the same background as the header. This is to
  // prevent personal color themes from showing in the side panel when
  // navigating to a new Lens results panel.
  webview->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorWindowBackground));
  return webview;
}

std::unique_ptr<views::ImageButton> CreateControlButton(
    views::View* host,
    base::RepeatingClosure pressed_callback,
    const gfx::VectorIcon& icon,
    const gfx::Insets& margin_insets,
    const std::u16string& tooltip_text,
    int dip_size) {
  auto button = views::CreateVectorImageButtonWithNativeTheme(pressed_callback,
                                                              icon, dip_size);
  button->SetTooltipText(tooltip_text);
  button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  button->SetProperty(views::kMarginsKey, margin_insets);
  // Make sure the hover background behind the button is a circle, rather than a
  // rounded square.
  views::InstallCircleHighlightPathGenerator(button.get());
  return button;
}

}  // namespace

namespace lens {

constexpr int kDefaultSidePanelHeaderHeight = 40;
constexpr int kGoogleLensLogoWidth = 87;
constexpr int kGoogleLensLogoHeight = 16;
const char kStaticGhostCardDataURL[] =
    "data:text/html;charset=utf-8,"
    "<!DOCTYPE html>"
    "<style>"
    "html, body {"
    "background-image: url('https://www.gstatic.com/lens/web/ui/side_panel_loading.gif');"
    "}</style>";

LensSidePanelView::LensSidePanelView(content::BrowserContext* browser_context,
                                     base::RepeatingClosure close_callback,
                                     base::RepeatingClosure launch_callback) {
  // Align views vertically top to bottom.
  SetOrientation(views::LayoutOrientation::kVertical);
  SetMainAxisAlignment(views::LayoutAlignment::kStart);
  // Stretch views to fill horizontal bounds.
  SetCrossAxisAlignment(views::LayoutAlignment::kStretch);
  CreateAndInstallHeader(close_callback, launch_callback);
  separator_ = AddChildView(std::make_unique<views::Separator>());
  loading_indicator_web_view_ = AddChildView(CreateWebView(this, browser_context));
  loading_indicator_web_view_->GetWebContents()->GetController().LoadURL(
        GURL(kStaticGhostCardDataURL), content::Referrer(), ui::PAGE_TRANSITION_FROM_API,
        std::string());
  web_view_ = AddChildView(CreateWebView(this, browser_context));
  web_view_->SetVisible(false);
}

content::WebContents* LensSidePanelView::GetWebContents() {
  return web_view_->GetWebContents();
}

void LensSidePanelView::OnThemeChanged() {
  views::FlexLayoutView::OnThemeChanged();
  const auto* color_provider = GetColorProvider();

  // kGoogleLensFullLogoIcon is rectangular. We should create a tiled image so
  // that the coordinates and scale are correct. The vector icon should have its
  // own fill color. The same applies to the dark mode icon.
  const SkColor color = color_provider->GetColor(ui::kColorIcon);
  const gfx::VectorIcon& icon = GetNativeTheme()->ShouldUseDarkColors()
                                    ? vector_icons::kGoogleLensFullLogoDarkIcon
                                    : vector_icons::kGoogleLensFullLogoIcon;
  const gfx::ImageSkia image = gfx::ImageSkiaOperations::CreateTiledImage(
      gfx::CreateVectorIcon(icon, color), 0, 0, kGoogleLensLogoWidth,
      kGoogleLensLogoHeight);
  branding_->SetImage(image);
}

void LensSidePanelView::CreateAndInstallHeader(
    base::RepeatingClosure close_callback,
    base::RepeatingClosure launch_callback) {
  auto header = std::make_unique<views::FlexLayoutView>();
  // ChromeLayoutProvider for providing margins.
  ChromeLayoutProvider* const chrome_layout_provider =
      ChromeLayoutProvider::Get();

  // Set the interior margins of the header on the left and right sides.
  header->SetInteriorMargin(gfx::Insets::TLBR(
      0,
      chrome_layout_provider->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_CONTROL_HORIZONTAL),
      0,
      chrome_layout_provider->GetDistanceMetric(
          ChromeDistanceMetric::
              DISTANCE_SIDE_PANEL_HEADER_INTERIOR_MARGIN_HORIZONTAL)));
  // Set alignments for horizontal (main) and vertical (cross) axes.
  header->SetMainAxisAlignment(views::LayoutAlignment::kStart);
  header->SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

  // The minimum cross axis size should the expected height of the header.
  header->SetMinimumCrossAxisSize(kDefaultSidePanelHeaderHeight);
  header->SetBackground(
      views::CreateThemedSolidBackground(ui::kColorWindowBackground));

  // Create Google Lens Logo branding.
  branding_ = header->AddChildView(std::make_unique<views::ImageView>());

  // Create an empty view between branding and buttons to align branding on left
  // without hardcoding margins. This view fills up the empty space between the
  // branding and the control buttons.
  auto container = std::make_unique<views::View>();
  container->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  header->AddChildView(std::move(container));

  launch_button_ = header->AddChildView(CreateControlButton(
      this, launch_callback, views::kLaunchIcon,
      gfx::Insets::TLBR(
          0, 0, 0,
          chrome_layout_provider->GetDistanceMetric(
              views::DistanceMetric::DISTANCE_CLOSE_BUTTON_MARGIN)),
      l10n_util::GetStringUTF16(IDS_ACCNAME_OPEN),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));
  close_button_ = header->AddChildView(CreateControlButton(
      this, close_callback, views::kIcCloseIcon, gfx::Insets(),
      l10n_util::GetStringUTF16(IDS_ACCNAME_CLOSE),
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_SIDE_PANEL_HEADER_VECTOR_ICON_SIZE)));

  // Install header.
  AddChildView(std::move(header));
}

void LensSidePanelView::SetContentAndNewTabButtonVisible(
    bool visible,
    bool enable_new_tab_button) {
  web_view_->SetVisible(visible);
  loading_indicator_web_view_->SetVisible(!visible);
  launch_button_->SetEnabled(enable_new_tab_button);
}

LensSidePanelView::~LensSidePanelView() = default;

}  // namespace lens
