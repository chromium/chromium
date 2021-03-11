// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/omnibox/omnibox_theme.h"
#include "chrome/browser/ui/page_info/page_info_dialog.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/url_formatter/url_formatter.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_constants.h"
#include "ui/native_theme/native_theme.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/image_button_factory.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/metadata/metadata_impl_macros.h"
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ui/base/chromeos_ui_constants.h"
#else
#include "chrome/browser/themes/theme_properties.h"
#endif

namespace {

bool ShouldDisplayUrl(content::WebContents* contents) {
  auto* tab_helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  if (tab_helper && tab_helper->IsDisplayingInterstitial())
    return tab_helper->ShouldDisplayURL();
  return true;
}

bool IsInitialUrlInAppScope(web_app::AppBrowserController* app_controller) {
  return app_controller
             ? app_controller->IsUrlInAppScope(app_controller->initial_url())
             : false;
}

bool IsUrlInAppScope(web_app::AppBrowserController* app_controller, GURL url) {
  return app_controller ? app_controller->IsUrlInAppScope(url) : false;
}

// TODO(tluk): The color id selection logic for security levels should be shared
// with that in GetOmniboxSecurityChipColor() once transition to Color Pipeline
// is complete.
ui::NativeTheme::ColorId GetSecurityChipColorId(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
      return ui::NativeTheme::kColorId_CustomTabBarSecurityChipWithCertColor;
    case security_state::SECURE:
      return ui::NativeTheme::kColorId_CustomTabBarSecurityChipSecureColor;
    case security_state::DANGEROUS:
      return ui::NativeTheme::kColorId_CustomTabBarSecurityChipDangerousColor;
    default:
      return ui::NativeTheme::kColorId_CustomTabBarSecurityChipDefaultColor;
  }
}

}  // namespace

// Container view for laying out and rendering the title/origin of the current
// page.
class CustomTabBarTitleOriginView : public views::View {
 public:
  METADATA_HEADER(CustomTabBarTitleOriginView);
  CustomTabBarTitleOriginView(SkColor background_color,
                              bool should_show_title) {
    auto location_label = std::make_unique<views::Label>(
        std::u16string(), views::style::CONTEXT_LABEL,
        views::style::STYLE_SECONDARY,
        gfx::DirectionalityMode::DIRECTIONALITY_AS_URL);

    location_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);
    location_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    location_label->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));
    location_label_ = AddChildView(std::move(location_label));

    if (should_show_title) {
      auto title_label = std::make_unique<views::Label>(
          std::u16string(), views::style::CONTEXT_LABEL);

      title_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
      title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
      title_label->SetProperty(
          views::kFlexBehaviorKey,
          views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kPreferred));
      title_label_ = AddChildView(std::move(title_label));
    }

    auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  }

  void Update(const std::u16string title, const std::u16string location) {
    if (title_label_)
      title_label_->SetText(title);
    location_label_->SetText(location);
    location_label_->SetVisible(!location.empty());
  }

  void SetColors(SkColor background_color) {
    if (title_label_)
      title_label_->SetBackgroundColor(background_color);
    location_label_->SetBackgroundColor(background_color);
  }

  int GetMinimumWidth() const {
    // As labels are not multi-line, the layout will calculate a minimum size
    // that would fit the entire text (potentially a long url). Instead, set a
    // minimum number of characters we want to display and elide the text if it
    // overflows.
    // This is in a helper function because we also have to ensure that the
    // preferred size is at least as wide as the minimum size, and the
    // minimum height of the control should be the preferred height.
    constexpr int kMinCharacters = 20;
    return title_label_
               ? title_label_->font_list().GetExpectedTextWidth(kMinCharacters)
               : location_label_->font_list().GetExpectedTextWidth(
                     kMinCharacters);
  }

  SkColor GetLocationColor() const {
    return views::style::GetColor(*this, CONTEXT_DIALOG_BODY_TEXT_SMALL,
                                  views::style::TextStyle::STYLE_PRIMARY);
  }

  // views::View:
  gfx::Size GetMinimumSize() const override {
    return gfx::Size(GetMinimumWidth(), GetPreferredSize().height());
  }

  gfx::Size CalculatePreferredSize() const override {
    // If we don't also override CalculatePreferredSize, we violate some
    // assumptions in the FlexLayout (that our PreferredSize is always larger
    // than our MinimumSize).
    gfx::Size preferred_size = views::View::CalculatePreferredSize();
    preferred_size.SetToMax(gfx::Size(GetMinimumWidth(), 0));
    return preferred_size;
  }

  bool IsShowingOriginForTesting() const {
    return location_label_ && location_label_->GetVisible();
  }

 private:
  // Can be nullptr.
  views::Label* title_label_ = nullptr;

  views::Label* location_label_ = nullptr;
};

BEGIN_METADATA(CustomTabBarTitleOriginView, views::View)
ADD_READONLY_PROPERTY_METADATA(int, MinimumWidth)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               LocationColor,
                               views::metadata::SkColorConverter)
END_METADATA

CustomTabBarView::CustomTabBarView(BrowserView* browser_view,
                                   LocationBarView::Delegate* delegate)
    : delegate_(delegate), browser_(browser_view->browser()) {
  set_context_menu_controller(this);

  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);

  close_button_ =
      AddChildView(views::CreateVectorImageButton(base::BindRepeating(
          &CustomTabBarView::GoBackToApp, base::Unretained(this))));
  close_button_->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button_->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING))));
  close_button_->SizeToPreferredSize();
  close_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
  views::InstallCircleHighlightPathGenerator(close_button_);

  location_icon_view_ =
      AddChildView(std::make_unique<LocationIconView>(font_list, this, this));

  auto title_origin_view = std::make_unique<CustomTabBarTitleOriginView>(
      background_color_, GetShowTitle());
  title_origin_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToMinimum,
                               views::MaximumFlexSizeRule::kUnbounded));
  title_origin_view_ = AddChildView(std::move(title_origin_view));

  // (TODO): This value can change, e.g. when changing from clamshell to tablet
  // mode. Find a better place to set it.
  gfx::Insets interior_margin =
      GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (browser_->is_type_custom_tab()) {
    web_app_menu_button_ = AddChildView(std::make_unique<WebAppMenuButton>(
        browser_view, l10n_util::GetStringUTF16(
                          IDS_CUSTOM_TABS_ACTION_MENU_ACCESSIBLE_NAME)));

    // Remove the vertical portion of the interior margin here to avoid
    // increasing the height of the toolbar when |web_app_menu_button_| is drawn
    // while maintaining its touch area.
    interior_margin.set_top(0);
    interior_margin.set_bottom(0);
  }
#endif

  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStretch)
      .SetInteriorMargin(interior_margin);

  browser_->tab_strip_model()->AddObserver(this);
}

CustomTabBarView::~CustomTabBarView() {}

gfx::Rect CustomTabBarView::GetAnchorBoundsInScreen() const {
  return gfx::UnionRects(location_icon_view_->GetAnchorBoundsInScreen(),
                         title_origin_view_->GetAnchorBoundsInScreen());
}

void CustomTabBarView::SetVisible(bool visible) {
  if (!GetVisible() && visible) {
    UpdateContents();
  }
  View::SetVisible(visible);
}

gfx::Size CustomTabBarView::CalculatePreferredSize() const {
  // ToolbarView::GetMinimumSize() uses the preferred size of its children, so
  // tell it the minimum size this control will fit into (its layout will
  // automatically have this control fill available space).
  return gfx::Size(layout_manager_->interior_margin().width() +
                       title_origin_view_->GetMinimumSize().width() +
                       close_button_->GetPreferredSize().width() +
                       location_icon_view_->GetPreferredSize().width(),
                   GetLayoutManager()->GetPreferredSize(this).height());
}

void CustomTabBarView::OnPaintBackground(gfx::Canvas* canvas) {
  views::View::OnPaintBackground(canvas);

  SkColor separator_color =
      color_utils::IsDark(background_color_) ? SK_ColorWHITE : SK_ColorBLACK;
  constexpr float kSeparatorOpacity = 0.15f;

  gfx::Rect bounds = GetLocalBounds();
  const gfx::Size separator_size = gfx::Size(bounds.width(), 1);

  // Inset the bounds by 1 on the bottom, so we draw the bottom border inside
  // the custom tab bar.
  bounds.Inset(0, 0, 0, 1);

  // Custom tab/content separator (bottom border).
  canvas->FillRect(gfx::Rect(bounds.bottom_left(), separator_size),
                   color_utils::AlphaBlend(separator_color, background_color_,
                                           kSeparatorOpacity));

  // Don't render the separator if there is already sufficient contrast between
  // the custom tab bar and the title bar.
  constexpr float kMaxContrastForSeparator = 1.1f;
  if (color_utils::GetContrastRatio(background_color_, title_bar_color_) >
      kMaxContrastForSeparator) {
    return;
  }

  // Frame/Custom tab separator (top border).
  canvas->FillRect(gfx::Rect(bounds.origin(), separator_size),
                   color_utils::AlphaBlend(separator_color, title_bar_color_,
                                           kSeparatorOpacity));
}

void CustomTabBarView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
  SchedulePaint();
}

void CustomTabBarView::OnThemeChanged() {
  views::AccessiblePaneView::OnThemeChanged();
  base::Optional<SkColor> optional_theme_color = GetThemeColor();

  title_bar_color_ = optional_theme_color.value_or(GetDefaultFrameColor());

  const SkColor foreground_color = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_CustomTabBarForegroundColor);
  SetImageFromVectorIconWithColor(
      close_button_, vector_icons::kCloseRoundedIcon,
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE), foreground_color);

  background_color_ = GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_CustomTabBarBackgroundColor);
  SetBackground(views::CreateSolidBackground(background_color_));

  title_origin_view_->SetColors(background_color_);
  if (web_app_menu_button_) {
    web_app_menu_button_->SetColor(GetThemeProvider()->GetColor(
        ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON));
  }
}

void CustomTabBarView::TabChangedAt(content::WebContents* contents,
                                    int index,
                                    TabChangeType change_type) {
  if (delegate_->GetWebContents() == contents)
    UpdateContents();
}

void CustomTabBarView::UpdateContents() {
  // If the toolbar should not be shown don't update the UI, as the toolbar may
  // be animating out and it looks messy.
  web_app::AppBrowserController* const app_controller =
      browser_->app_controller();
  if (app_controller && !app_controller->ShouldShowCustomTabBar())
    return;

  content::WebContents* contents = delegate_->GetWebContents();
  if (!contents)
    return;

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  std::u16string title, location;
  if (entry) {
    title = Browser::FormatTitleForDisplay(entry->GetTitleForDisplay());
    if (ShouldDisplayUrl(contents)) {
      location = web_app::AppBrowserController::FormatUrlOrigin(
          contents->GetVisibleURL(), url_formatter::kFormatUrlOmitDefaults);
    }
  }

  title_origin_view_->Update(title, location);
  location_icon_view_->Update(/*suppress animations = */ false);

  // Hide location icon if we're already hiding the origin.
  location_icon_view_->SetVisible(!location.empty());

  last_title_ = title;
  last_location_ = location;

  // Only show the 'X' button if:
  // a) The current url is not in scope (no point showing a back to app button
  // while in scope).
  // And b), if the window started in scope (this is
  // important for popup windows, which may be opened outside the app).
  bool set_visible =
      IsInitialUrlInAppScope(app_controller) &&
      !IsUrlInAppScope(app_controller, contents->GetLastCommittedURL());
  close_button_->SetVisible(set_visible);

  Layout();
}

SkColor CustomTabBarView::GetIconLabelBubbleSurroundingForegroundColor() const {
  return title_origin_view_->GetLocationColor();
}

SkColor CustomTabBarView::GetIconLabelBubbleBackgroundColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_CustomTabBarBackgroundColor);
}

content::WebContents* CustomTabBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

bool CustomTabBarView::IsEditingOrEmpty() const {
  return false;
}

void CustomTabBarView::OnLocationIconPressed(const ui::MouseEvent& event) {}

void CustomTabBarView::OnLocationIconDragged(const ui::MouseEvent& event) {}

SkColor CustomTabBarView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  return GetNativeTheme()->GetSystemColor(
      GetSecurityChipColorId(security_level));
}

bool CustomTabBarView::ShowPageInfoDialog() {
  return ::ShowPageInfoDialog(
      GetWebContents(),
      base::BindOnce(&CustomTabBarView::AppInfoClosedCallback,
                     weak_factory_.GetWeakPtr()),
      bubble_anchor_util::Anchor::kCustomTabBar);
}

const LocationBarModel* CustomTabBarView::GetLocationBarModel() const {
  return delegate_->GetLocationBarModel();
}

ui::ImageModel CustomTabBarView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  return ui::ImageModel::FromVectorIcon(
      delegate_->GetLocationBarModel()->GetVectorIcon(),
      GetSecurityChipColor(GetLocationBarModel()->GetSecurityLevel()),
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE));
}

void CustomTabBarView::GoBackToAppForTesting() {
  GoBackToApp();
}

bool CustomTabBarView::IsShowingOriginForTesting() const {
  return title_origin_view_ && title_origin_view_->IsShowingOriginForTesting();
}

// TODO(tluk): Remove the use of GetDefaultFrameColor() completely here. When
// drawing the separator the current frame color should be queried directly and
// not assume knowledge of what the color might be.
SkColor CustomTabBarView::GetDefaultFrameColor() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Ash system frames differ from ChromeOS browser frames.
  return chromeos::kDefaultFrameColor;
#else
  return ThemeProperties::GetDefaultColor(
      ThemeProperties::COLOR_FRAME_ACTIVE, false,
      GetNativeTheme()->ShouldUseDarkColors());
#endif
}

void CustomTabBarView::GoBackToApp() {
  content::WebContents* web_contents = GetWebContents();
  content::NavigationController& controller = web_contents->GetController();

  content::NavigationEntry* entry = nullptr;
  int offset = 0;
  web_app::AppBrowserController* application_controller = app_controller();

  // Go back until we find an in scope url or run out of urls.
  while ((entry = controller.GetEntryAtOffset(offset)) &&
         !IsUrlInAppScope(application_controller, entry->GetURL())) {
    offset--;
  }

  // If there are no in scope urls, push the app's launch url and clear
  // the history.
  if (!entry) {
    if (application_controller) {
      GURL initial_url = application_controller->GetAppStartUrl();
      content::NavigationController::LoadURLParams load(initial_url);
      load.should_clear_history_list = true;
      controller.LoadURLWithParams(load);
    }
    return;
  }

  // Otherwise, go back to the first in scope url.
  controller.GoToOffset(offset);
}

void CustomTabBarView::AppInfoClosedCallback(
    views::Widget::ClosedReason closed_reason,
    bool reload_prompt) {
  // If we're closing the bubble because the user pressed ESC or because the
  // user clicked Close (rather than the user clicking directly on something
  // else), we should refocus the location bar. This lets the user tab into the
  // "You should reload this page" infobar rather than dumping them back out
  // into a stale webpage.
  if (!reload_prompt)
    return;
  if (closed_reason != views::Widget::ClosedReason::kEscKeyPressed &&
      closed_reason != views::Widget::ClosedReason::kCloseButtonClicked) {
    return;
  }

  GetFocusManager()->SetFocusedView(location_icon_view_);
}

void CustomTabBarView::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_COPY_URL) {
    base::RecordAction(base::UserMetricsAction("CopyCustomTabBarUrl"));
    chrome::ExecuteCommand(browser_, command_id);
  }
}

void CustomTabBarView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  if (!context_menu_model_) {
    context_menu_model_ = std::make_unique<ui::SimpleMenuModel>(this);
    context_menu_model_->AddItemWithStringId(IDC_COPY_URL, IDS_COPY_URL);
  }
  context_menu_runner_ = std::make_unique<views::MenuRunner>(
      context_menu_model_.get(),
      views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU);
  context_menu_runner_->RunMenuAt(
      views::View::GetWidget(), nullptr, gfx::Rect(point, gfx::Size()),
      views::MenuAnchorPosition::kTopLeft, source_type);
}

base::Optional<SkColor> CustomTabBarView::GetThemeColor() const {
  web_app::AppBrowserController* application_controller = app_controller();
  return application_controller ? application_controller->GetThemeColor()
                                : base::nullopt;
}

bool CustomTabBarView::GetShowTitle() const {
  return app_controller() != nullptr;
}

BEGIN_METADATA(CustomTabBarView, views::AccessiblePaneView)
ADD_READONLY_PROPERTY_METADATA(SkColor,
                               DefaultFrameColor,
                               views::metadata::SkColorConverter)
ADD_READONLY_PROPERTY_METADATA(base::Optional<SkColor>, ThemeColor)
ADD_READONLY_PROPERTY_METADATA(bool, ShowTitle)
END_METADATA
