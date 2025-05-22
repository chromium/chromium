// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"

#include <optional>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/grit/generated_resources.h"
#include "components/url_formatter/url_formatter.h"
#include "multi_contents_view_mini_toolbar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace {
constexpr int kContentOutlineThickness = 1;
constexpr int kMiniToolbarContentPadding = 4;
constexpr int kMiniToolbarOutlineCornerRadius = 8;
}  // namespace

MultiContentsViewMiniToolbar::MultiContentsViewMiniToolbar(
    BrowserView* browser_view,
    ContentsWebView* web_view)
    : menu_model_(std::make_unique<SplitTabMenuModel>(
          browser_view->browser()->tab_strip_model())),
      browser_view_(browser_view),
      web_contents_(web_view->GetWebContents()) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(gfx::Insets::TLBR(
          kMiniToolbarOutlineCornerRadius + kMiniToolbarContentPadding,
          kMiniToolbarOutlineCornerRadius * 2, kMiniToolbarContentPadding,
          kContentOutlineThickness))
      .SetDefault(views::kMarginsKey, gfx::Insets::VH(0, 6))
      .SetIgnoreDefaultMainAxisMargins(true)
      .SetCollapseMargins(true);
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetVisible(false);

  // Add favicon, domain label, alert state indicator, and menu button.
  favicon_ = AddChildView(std::make_unique<views::ImageView>());
  const views::FlexSpecification icon_flex_spec =
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferredSnapToZero,
                               views::MaximumFlexSizeRule::kPreferred);
  favicon_->SetProperty(views::kFlexBehaviorKey, icon_flex_spec.WithOrder(3));
  domain_label_ = AddChildView(std::make_unique<views::Label>());
  domain_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(4));
  domain_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  domain_label_->SetTruncateLength(20);
  domain_label_->SetSubpixelRenderingEnabled(false);
  alert_state_indicator_ = AddChildView(std::make_unique<views::ImageView>());
  alert_state_indicator_->SetProperty(views::kFlexBehaviorKey,
                                      icon_flex_spec.WithOrder(2));
  menu_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
      base::RepeatingClosure(), kBrowserToolsChromeRefreshIcon, 16,
      kColorSidePanelHeaderButtonIcon,
      kColorSidePanelHeaderButtonIconDisabled));
  menu_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));
  menu_button_->SetTooltipText(l10n_util::GetStringUTF16(
      IDS_SPLIT_VIEW_MINI_TOOLBAR_MENU_BUTTON_TOOLTIP));
  views::InstallCircleHighlightPathGenerator(menu_button_);
  menu_button_->SetButtonController(
      std::make_unique<views::MenuButtonController>(
          menu_button_,
          base::BindRepeating(&MultiContentsViewMiniToolbar::OpenSplitViewMenu,
                              base::Unretained(this)),
          std::make_unique<views::Button::DefaultButtonControllerDelegate>(
              menu_button_)));

  // Update minitoolbar contents.
  std::optional<TabRendererData> tab_data = GetTabData();
  if (tab_data.has_value()) {
    UpdateContents(tab_data.value());
  }

  web_contents_attached_subscription_ =
      web_view->AddWebContentsAttachedCallback(
          base::BindRepeating(&MultiContentsViewMiniToolbar::UpdateWebContents,
                              base::Unretained(this)));
  web_contents_detached_subscription_ =
      web_view->AddWebContentsDetachedCallback(
          base::BindRepeating(&MultiContentsViewMiniToolbar::ClearWebContents,
                              base::Unretained(this)));
  browser_view_->browser()->tab_strip_model()->AddObserver(this);
}

MultiContentsViewMiniToolbar::~MultiContentsViewMiniToolbar() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
}

void MultiContentsViewMiniToolbar::UpdateWebContents(views::WebView* web_view) {
  web_contents_ = web_view->GetWebContents();
  std::optional<TabRendererData> tab_data = GetTabData();
  if (tab_data.has_value()) {
    UpdateContents(tab_data.value());
  }
}

void MultiContentsViewMiniToolbar::ClearWebContents(views::WebView*) {
  web_contents_ = nullptr;
}

void MultiContentsViewMiniToolbar::TabChangedAt(content::WebContents* contents,
                                                int index,
                                                TabChangeType change_type) {
  if (!web_contents_ || contents != web_contents_) {
    return;
  }
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  TabRendererData tab_data = TabRendererData::FromTabInModel(model, index);
  UpdateContents(tab_data);
}

void MultiContentsViewMiniToolbar::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  // Clip the curved inner side of the mini toolbar.
  const gfx::Rect local_bounds = GetLocalBounds();
  SkPath path;
  const float corner_radius = kMiniToolbarOutlineCornerRadius;
  path.moveTo(0, local_bounds.height());
  path.lineTo(0, local_bounds.height() - kContentOutlineThickness);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCCW, corner_radius,
             local_bounds.height() - corner_radius);
  path.lineTo(corner_radius, corner_radius * 2);
  path.arcTo(corner_radius, corner_radius, 270.0f, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, corner_radius * 2, corner_radius);
  path.lineTo(local_bounds.width() - corner_radius, corner_radius);
  path.arcTo(8, 8, 0, SkPath::kSmall_ArcSize, SkPathDirection::kCCW,
             local_bounds.width() - kContentOutlineThickness, 0);
  path.lineTo(local_bounds.width(), 0);
  path.lineTo(local_bounds.width(), local_bounds.height());
  path.lineTo(0, local_bounds.height());
  SetClipPath(path);
}

void MultiContentsViewMiniToolbar::OnPaint(gfx::Canvas* canvas) {
  // Paint the mini toolbar background to match the toolbar.
  TopContainerBackground::PaintBackground(canvas, this, browser_view_);

  // Draw the bordering stroke.
  cc::PaintFlags flags;
  flags.setStrokeWidth(kContentOutlineThickness * 2);
  flags.setColor(GetColorProvider()->GetColor(
      kColorMulitContentsViewInactiveContentOutline));
  flags.setStyle(cc::PaintFlags::kStroke_Style);
  flags.setAntiAlias(true);
  const float corner_radius = kMiniToolbarOutlineCornerRadius;
  const gfx::Rect local_bounds = GetLocalBounds();
  SkPath path;
  path.moveTo(0, local_bounds.height() - kContentOutlineThickness);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCCW, corner_radius,
             local_bounds.height() - corner_radius);
  path.lineTo(corner_radius, corner_radius * 2);
  path.arcTo(corner_radius, corner_radius, 270.0f, SkPath::kSmall_ArcSize,
             SkPathDirection::kCW, corner_radius * 2, corner_radius);
  path.lineTo(local_bounds.width() - corner_radius, corner_radius);
  path.arcTo(corner_radius, corner_radius, 0, SkPath::kSmall_ArcSize,
             SkPathDirection::kCCW,
             local_bounds.width() - kContentOutlineThickness, 0);
  canvas->DrawPath(path, flags);
}

void MultiContentsViewMiniToolbar::OnThemeChanged() {
  views::View::OnThemeChanged();
  std::optional<TabRendererData> tab_data = GetTabData();
  if (tab_data.has_value()) {
    UpdateFavicon(tab_data.value());
  }
}

std::optional<TabRendererData> MultiContentsViewMiniToolbar::GetTabData() {
  if (!web_contents_) {
    return std::nullopt;
  }
  TabStripModel* model = browser_view_->browser()->tab_strip_model();
  int tab_index = model->GetIndexOfWebContents(web_contents_);
  if (tab_index == TabStripModel::kNoTab) {
    return std::nullopt;
  }
  return TabRendererData::FromTabInModel(model, tab_index);
}

void MultiContentsViewMiniToolbar::UpdateContents(TabRendererData tab_data) {
  GURL domain_url = tab_data.visible_url;
  if (tab_data.last_committed_url.is_valid()) {
    domain_url = tab_data.last_committed_url;
  }
  // Create the formatted domain, this will match the hover card domain.
  std::u16string domain;
  if (domain_url.SchemeIsFile()) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else if (domain_url.SchemeIsBlob()) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
  } else if (tab_data.should_display_url) {
    domain = url_formatter::FormatUrl(
        domain_url,
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
  }
  domain_label_->SetText(domain);

  UpdateFavicon(tab_data);
}

void MultiContentsViewMiniToolbar::UpdateFavicon(TabRendererData tab_data) {
  // Theme the favicon similar to how favicons are themed in the bookmarks bar.
  ui::ImageModel favicon = tab_data.favicon;
  bool themify_favicon = tab_data.should_themify_favicon;
  if (favicon.IsEmpty()) {
    favicon = favicon::GetDefaultFaviconModel(kColorBookmarkBarBackground);
    themify_favicon = true;
  }
  if (const auto* provider = GetColorProvider(); provider && themify_favicon) {
    SkColor favicon_color = provider->GetColor(kColorBookmarkFavicon);
    if (favicon_color != SK_ColorTRANSPARENT) {
      favicon = ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateColorMask(favicon.Rasterize(provider),
                                                    favicon_color));
    }
  }
  favicon_->SetImage(favicon);
}

void MultiContentsViewMiniToolbar::OpenSplitViewMenu() {
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(menu_button_->GetWidget(),
                          static_cast<views::MenuButtonController*>(
                              menu_button_->button_controller()),
                          menu_button_->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kTopRight,
                          ui::mojom::MenuSourceType::kNone);
}

BEGIN_METADATA(MultiContentsViewMiniToolbar)
END_METADATA
