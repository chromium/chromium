// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_mini_toolbar.h"

#include <optional>

#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/tabs/alert/tab_alert.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_controller.h"
#include "chrome/browser/ui/tabs/alert/tab_alert_icon.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/contents_container_outline.h"
#include "chrome/browser/ui/views/frame/contents_web_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/base/models/menu_model.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
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
constexpr int kMiniToolbarContentPadding = 4;
constexpr int kMiniToolbarDomainMaxWidth = 140;

tabs::TabInterface* GetTabInterface(content::WebContents* web_contents) {
  return web_contents ? tabs::TabInterface::MaybeGetFromContents(web_contents)
                      : nullptr;
}

bool IsNTP(const GURL& url) {
  return (url.SchemeIs(content::kChromeUIScheme) &&
          url.GetHost() == chrome::kChromeUINewTabHost) ||
         search::IsNTPURL(url) || search::IsSplitViewNewTabPage(url);
}

void SetAccessibleNameAndTooltip(views::View* view, int string_id) {
  std::u16string string = l10n_util::GetStringUTF16(string_id);
  view->SetAccessibleName(string);
  view->SetTooltipText(string);
}
}  // namespace

MultiContentsViewMiniToolbar::MultiContentsViewMiniToolbar(
    BrowserView* browser_view,
    ContentsWebView* web_view)
    : browser_view_(browser_view), web_contents_(web_view->web_contents()) {
  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
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
  domain_label_ = AddChildView(std::make_unique<views::Label>(
      u"", views::style::CONTEXT_LABEL, views::style::STYLE_PRIMARY,
      gfx::DirectionalityMode::DIRECTIONALITY_AS_URL));
  domain_label_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(
          views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(4));
  domain_label_->SetMaximumWidthSingleLine(kMiniToolbarDomainMaxWidth);
  domain_label_->SetElideBehavior(gfx::ELIDE_HEAD);
  domain_label_->SetSubpixelRenderingEnabled(false);
  domain_label_->SetEnabledColor(kColorMultiContentsViewMiniToolbarForeground);
  domain_label_->SetBackgroundColor(kColorToolbar);
  alert_state_indicator_ = AddChildView(std::make_unique<views::ImageView>());
  alert_state_indicator_->SetProperty(views::kFlexBehaviorKey,
                                      icon_flex_spec.WithOrder(2));
  if (features::kSideBySideMiniToolbarActiveConfiguration.Get() ==
      features::MiniToolbarActiveConfiguration::ShowClose) {
    image_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        base::BindRepeating(&MultiContentsViewMiniToolbar::CloseCurrentView,
                            base::Unretained(this)),
        kCloseTabChromeRefreshIcon, 16,
        kColorMultiContentsViewMiniToolbarForeground));
    SetAccessibleNameAndTooltip(image_button_, IDS_SPLIT_TAB_CLOSE);
  } else {
    image_button_ = AddChildView(views::CreateVectorImageButtonWithNativeTheme(
        base::RepeatingClosure(), kBrowserToolsChromeRefreshIcon, 16,
        kColorMultiContentsViewMiniToolbarForeground));
    SetAccessibleNameAndTooltip(
        image_button_, IDS_ACCNAME_SPLIT_VIEW_MINI_TOOLBAR_MENU_BUTTON);
    image_button_->SetButtonController(
        std::make_unique<views::MenuButtonController>(
            image_button_,
            base::BindRepeating(
                &MultiContentsViewMiniToolbar::OpenSplitViewMenu,
                base::Unretained(this)),
            std::make_unique<views::Button::DefaultButtonControllerDelegate>(
                image_button_)));
  }
  image_button_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                               views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1));
  views::InstallCircleHighlightPathGenerator(image_button_);

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

  RegisterTabAlertSubscription();

  browser_view_->browser()->tab_strip_model()->AddObserver(this);
}

MultiContentsViewMiniToolbar::~MultiContentsViewMiniToolbar() {
  browser_view_->browser()->tab_strip_model()->RemoveObserver(this);
}

void MultiContentsViewMiniToolbar::UpdateState(bool is_active,
                                               bool is_highlighted) {
  const int contents_container_outline_thickness =
      ContentsContainerOutline::GetThickness(is_highlighted);

  gfx::Insets kInactiveInteriorMargins = gfx::Insets::TLBR(
      ContentsContainerOutline::kCornerRadius + kMiniToolbarContentPadding,
      ContentsContainerOutline::kCornerRadius * 2,
      contents_container_outline_thickness,
      contents_container_outline_thickness);

  // Reduce the margins in the case of showing only the close or menu button.
  gfx::Insets kActiveInteriorMargins = gfx::Insets::TLBR(
      ContentsContainerOutline::kCornerRadius + kMiniToolbarContentPadding,
      ContentsContainerOutline::kCornerRadius + kMiniToolbarContentPadding,
      contents_container_outline_thickness,
      contents_container_outline_thickness);

  static_cast<views::FlexLayout*>(GetLayoutManager())
      ->SetInteriorMargin(is_active ? kActiveInteriorMargins
                                    : kInactiveInteriorMargins);

  if (features::kSideBySideMiniToolbarActiveConfiguration.Get() ==
      features::MiniToolbarActiveConfiguration::Hide) {
    SetVisible(!is_active);
    return;
  }

  SetVisible(!is_highlighted);

  favicon_->SetVisible(!is_active);
  domain_label_->SetVisible(!is_active);
  alert_state_indicator_->SetVisible(!is_active);
}

void MultiContentsViewMiniToolbar::UpdateContents() {
  std::optional<TabRendererData> tab_data = GetTabData();
  if (tab_data.has_value()) {
    UpdateContents(tab_data.value());
  }
}

void MultiContentsViewMiniToolbar::UpdateWebContents(views::WebView* web_view) {
  tab_alert_status_subscription_.reset();
  web_contents_ = web_view->web_contents();
  RegisterTabAlertSubscription();
  std::optional<TabRendererData> tab_data = GetTabData();
  if (tab_data.has_value()) {
    UpdateContents(tab_data.value());
  }
}

void MultiContentsViewMiniToolbar::ClearWebContents(views::WebView*) {
  tab_alert_status_subscription_.reset();
  OnAlertStatusIndicatorChanged(std::nullopt);
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

void MultiContentsViewMiniToolbar::OnPaint(gfx::Canvas* canvas) {
  // Paint the mini toolbar background to match the toolbar.
  TopContainerBackground::PaintBackground(canvas, this, browser_view_);
}

void MultiContentsViewMiniToolbar::OnThemeChanged() {
  views::View::OnThemeChanged();
  std::optional<TabRendererData> tab_data = GetTabData();
  if (tab_data.has_value()) {
    UpdateFavicon(tab_data.value());
  }
  if (auto* interface = GetTabInterface(web_contents_)) {
    auto* tab_alert_controller = tabs::TabAlertController::From(interface);
    if (tab_alert_controller) {
      OnAlertStatusIndicatorChanged(tab_alert_controller->GetAlertToShow());
    }
  }
}

void MultiContentsViewMiniToolbar::RegisterTabAlertSubscription() {
  if (auto* interface = GetTabInterface(web_contents_)) {
    auto* tab_alert_controller = tabs::TabAlertController::From(interface);
    OnAlertStatusIndicatorChanged(tab_alert_controller->GetAlertToShow());
    tab_alert_status_subscription_ =
        tab_alert_controller->AddAlertToShowChangedCallback(base::BindRepeating(
            &MultiContentsViewMiniToolbar::OnAlertStatusIndicatorChanged,
            base::Unretained(this)));
  }
}

void MultiContentsViewMiniToolbar::OnAlertStatusIndicatorChanged(
    std::optional<tabs::TabAlert> new_alert) {
  if (new_alert.has_value()) {
    ui::ColorId color = GetColorProvider() ? tabs::GetAlertIndicatorColor(
                                                 new_alert.value(), true, true)
                                           : gfx::kPlaceholderColor;
    alert_state_indicator_->SetImage(
        tabs::GetAlertImageModel(new_alert.value(), color));
    alert_state_indicator_->SetTooltipText(
        tabs::TabAlertController::GetTabAlertStateText(new_alert.value()));
  } else {
    alert_state_indicator_->SetImage(ui::ImageModel());
    alert_state_indicator_->SetTooltipText(std::u16string());
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
  if (IsNTP(domain_url)) {
    domain = u"";
  } else if (domain_url.SchemeIsFile()) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_FILE_URL_SOURCE);
  } else if (domain_url.SchemeIsBlob()) {
    domain = l10n_util::GetStringUTF16(IDS_HOVER_CARD_BLOB_URL_SOURCE);
  } else if (tab_data.should_display_url) {
    domain = url_formatter::FormatUrl(
        domain_url,
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlTrimAfterHost,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
  }
  domain_label_->SetText(domain);
  domain_label_->SetElideBehavior(domain_url.IsStandard() &&
                                          !domain_url.SchemeIsFile() &&
                                          !domain_url.SchemeIsFileSystem()
                                      ? gfx::ELIDE_HEAD
                                      : gfx::ELIDE_TAIL);
  domain_label_->SetCustomTooltipText(base::ASCIIToUTF16(domain_url.spec()));

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
  base::RecordAction(
      base::UserMetricsAction("DesktopSplitView_OpenMiniToolbarMenu"));

  TabStripModel* const model = browser_view_->browser()->tab_strip_model();
  const int index = model->GetIndexOfWebContents(web_contents_);
  menu_model_ = std::make_unique<SplitTabMenuModel>(
      browser_view_->browser()->tab_strip_model(),
      SplitTabMenuModel::MenuSource::kMiniToolbar, index);
  menu_runner_ = std::make_unique<views::MenuRunner>(
      menu_model_.get(), views::MenuRunner::HAS_MNEMONICS);
  menu_runner_->RunMenuAt(image_button_->GetWidget(),
                          static_cast<views::MenuButtonController*>(
                              image_button_->button_controller()),
                          image_button_->GetAnchorBoundsInScreen(),
                          views::MenuAnchorPosition::kBubbleTopLeft,
                          ui::mojom::MenuSourceType::kNone);
}

void MultiContentsViewMiniToolbar::CloseCurrentView() {
  base::RecordAction(
      base::UserMetricsAction("DesktopSplitView_MiniToolbarCloseView"));

  TabStripModel* const model = browser_view_->browser()->tab_strip_model();
  const int index = model->GetIndexOfWebContents(web_contents_);

  if (index == TabStripModel::kNoTab) {
    // Only close the WebContents if it exists. crbug.com/459828484
    return;
  }

  model->CloseWebContentsAt(index,
                            TabCloseTypes::CLOSE_USER_GESTURE |
                                TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
}

BEGIN_METADATA(MultiContentsViewMiniToolbar)
END_METADATA
