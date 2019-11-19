// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/custom_tab_bar_view.h"

#include <memory>

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
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
#include "ui/views/style/typography.h"
#include "ui/views/style/typography_provider.h"
#include "ui/views/view_class_properties.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/ash_constants.h"
#else
#include "chrome/browser/themes/theme_properties.h"
#endif

namespace {

constexpr SkColor kDefaultCustomTabBarBackgroundColor = SK_ColorWHITE;

// The frame color is different on ChromeOS and other platforms because Ash
// specifies its own default frame color, which is not exposed through
// BrowserNonClientFrameView::GetFrameColor.
SkColor GetDefaultFrameColor() {
#if defined(OS_CHROMEOS)
  return ash::kDefaultFrameColor;
#else
  return ThemeProperties::GetDefaultColor(ThemeProperties::COLOR_FRAME, false);
#endif
}

std::unique_ptr<views::ImageButton> CreateCloseButton(
    views::ButtonListener* listener,
    SkColor color) {
  auto close_button = CreateVectorImageButton(listener);
  SetImageFromVectorIconWithColor(
      close_button.get(), vector_icons::kCloseRoundedIcon,
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE), color);
  close_button->SetTooltipText(l10n_util::GetStringUTF16(IDS_APP_CLOSE));
  close_button->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(GetLayoutConstant(LOCATION_BAR_CHILD_INTERIOR_PADDING))));
  close_button->SizeToPreferredSize();

  views::InstallCircleHighlightPathGenerator(close_button.get());

  return close_button;
}

bool ShouldDisplayUrl(content::WebContents* contents) {
  auto* tab_helper =
      security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
          contents);
  if (tab_helper && tab_helper->IsDisplayingInterstitial())
    return tab_helper->ShouldDisplayURL();
  return true;
}

}  // namespace

// Container view for laying out and rendering the title/origin of the current
// page.
class CustomTabBarTitleOriginView : public views::View {
 public:
  explicit CustomTabBarTitleOriginView(SkColor background_color) {
    auto title_label = std::make_unique<views::Label>(
        base::string16(), CONTEXT_BODY_TEXT_LARGE,
        views::style::TextStyle::STYLE_PRIMARY);
    auto location_label = std::make_unique<views::Label>(
        base::string16(), CONTEXT_BODY_TEXT_SMALL,
        views::style::STYLE_SECONDARY,
        gfx::DirectionalityMode::DIRECTIONALITY_AS_URL);

    title_label->SetBackgroundColor(background_color);
    title_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
    title_label->SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_LEFT);
    title_label->SetProperty(views::kFlexBehaviorKey,
                             views::FlexSpecification::ForSizeRule(
                                 views::MinimumFlexSizeRule::kScaleToMinimum,
                                 views::MaximumFlexSizeRule::kPreferred));

    location_label->SetBackgroundColor(background_color);
    location_label->SetElideBehavior(gfx::ElideBehavior::ELIDE_HEAD);
    location_label->SetHorizontalAlignment(
        gfx::HorizontalAlignment::ALIGN_LEFT);
    location_label->SetProperty(views::kFlexBehaviorKey,
                                views::FlexSpecification::ForSizeRule(
                                    views::MinimumFlexSizeRule::kScaleToMinimum,
                                    views::MaximumFlexSizeRule::kPreferred));

    title_label_ = AddChildView(std::move(title_label));
    location_label_ = AddChildView(std::move(location_label));

    auto* layout = SetLayoutManager(std::make_unique<views::FlexLayout>());
    layout->SetOrientation(views::LayoutOrientation::kVertical)
        .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
        .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  }

  void Update(base::string16 title, base::string16 location) {
    title_label_->SetText(title);
    location_label_->SetText(location);
    location_label_->SetVisible(!location.empty());
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
    return title_label_->font_list().GetExpectedTextWidth(kMinCharacters);
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
    return location_label_ != nullptr && location_label_->GetVisible();
  }

 private:
  views::Label* title_label_;
  views::Label* location_label_;
};

// static
const char CustomTabBarView::kViewClassName[] = "CustomTabBarView";

CustomTabBarView::CustomTabBarView(BrowserView* browser_view,
                                   LocationBarView::Delegate* delegate)
    : TabStripModelObserver(),
      delegate_(delegate),
      browser_(browser_view->browser()) {
  set_context_menu_controller(this);
  base::Optional<SkColor> optional_theme_color =
      browser_->app_controller()->GetThemeColor();

  // If we have a theme color, use that, otherwise fall back to the default
  // frame color.
  title_bar_color_ = optional_theme_color.value_or(GetDefaultFrameColor());

  // Match the default frame colors if using dark colors.
  background_color_ =
      ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
          ? GetDefaultFrameColor()
          : kDefaultCustomTabBarBackgroundColor;

  SetBackground(views::CreateSolidBackground(background_color_));

  const SkColor foreground_color =
      color_utils::GetColorWithMaxContrast(background_color_);

  const gfx::FontList& font_list = views::style::GetFont(
      CONTEXT_OMNIBOX_PRIMARY, views::style::STYLE_PRIMARY);

  close_button_ = AddChildView(CreateCloseButton(this, foreground_color));

  location_icon_view_ =
      AddChildView(std::make_unique<LocationIconView>(font_list, this));

  auto title_origin_view =
      std::make_unique<CustomTabBarTitleOriginView>(background_color_);
  title_origin_view->SetProperty(
      views::kFlexBehaviorKey, views::FlexSpecification::ForSizeRule(
                                   views::MinimumFlexSizeRule::kScaleToMinimum,
                                   views::MaximumFlexSizeRule::kPreferred));
  title_origin_view_ = AddChildView(std::move(title_origin_view));

  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());
  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetInteriorMargin(GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN));

  browser_->tab_strip_model()->AddObserver(this);
}

CustomTabBarView::~CustomTabBarView() {}

gfx::Rect CustomTabBarView::GetAnchorBoundsInScreen() const {
  return gfx::UnionRects(location_icon_view_->GetAnchorBoundsInScreen(),
                         title_origin_view_->GetAnchorBoundsInScreen());
}

const char* CustomTabBarView::GetClassName() const {
  return kViewClassName;
}

void CustomTabBarView::TabChangedAt(content::WebContents* contents,
                                    int index,
                                    TabChangeType change_type) {
  if (!contents)
    return;

  // If the toolbar should not be shown don't update the UI, as the toolbar may
  // be animating out and it looks messy.
  Browser* browser = chrome::FindBrowserWithWebContents(contents);
  if (!browser->app_controller()->ShouldShowCustomTabBar())
    return;

  content::NavigationEntry* entry = contents->GetController().GetVisibleEntry();
  base::string16 title, location;
  if (entry) {
    title = Browser::FormatTitleForDisplay(entry->GetTitleForDisplay());
    if (ShouldDisplayUrl(contents))
      location = url_formatter::FormatUrl(entry->GetVirtualURL().GetOrigin(),
                                          url_formatter::kFormatUrlOmitDefaults,
                                          net::UnescapeRule::NORMAL, nullptr,
                                          nullptr, nullptr);
  }

  title_origin_view_->Update(title, location);
  location_icon_view_->Update(/*suppress animations = */ false);

  // Hide location icon if we're already hiding the origin.
  location_icon_view_->SetVisible(!location.empty());

  last_title_ = title;
  last_location_ = location;

  web_app::AppBrowserController* app_controller =
      chrome::FindBrowserWithWebContents(contents)->app_controller();
  const bool started_in_scope =
      app_controller->IsUrlInAppScope(app_controller->initial_url());

  // Only show the 'X' button if:
  // a) The current url is not in scope (no point showing a back to app button
  // while in scope).
  // And b), if the window started in scope (this is
  // important for popup windows, which may be opened outside the app).
  close_button_->SetVisible(
      started_in_scope &&
      !app_controller->IsUrlInAppScope(contents->GetLastCommittedURL()));

  Layout();
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

void CustomTabBarView::ExecuteCommand(int command_id, int event_flags) {
  if (command_id == IDC_COPY_URL) {
    base::RecordAction(base::UserMetricsAction("CopyCustomTabBarUrl"));
    chrome::ExecuteCommand(browser_, command_id);
  }
}

content::WebContents* CustomTabBarView::GetWebContents() {
  return delegate_->GetWebContents();
}

bool CustomTabBarView::IsEditingOrEmpty() const {
  return false;
}

void CustomTabBarView::OnLocationIconPressed(const ui::MouseEvent& event) {}

void CustomTabBarView::OnLocationIconDragged(const ui::MouseEvent& event) {}

bool CustomTabBarView::ShowPageInfoDialog() {
  return ::ShowPageInfoDialog(
      GetWebContents(),
      base::BindOnce(&CustomTabBarView::AppInfoClosedCallback,
                     weak_factory_.GetWeakPtr()),
      bubble_anchor_util::Anchor::kCustomTabBar);
}

SkColor CustomTabBarView::GetSecurityChipColor(
    security_state::SecurityLevel security_level) const {
  return GetOmniboxSecurityChipColor(
      &ThemeService::GetThemeProviderForProfile(browser_->profile()),
      security_level);
}

gfx::ImageSkia CustomTabBarView::GetLocationIcon(
    LocationIconView::Delegate::IconFetchedCallback on_icon_fetched) const {
  return gfx::CreateVectorIcon(
      delegate_->GetLocationBarModel()->GetVectorIcon(),
      GetLayoutConstant(LOCATION_BAR_ICON_SIZE),
      GetSecurityChipColor(GetLocationBarModel()->GetSecurityLevel()));
}

SkColor CustomTabBarView::GetLocationIconInkDropColor() const {
  return GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_TextfieldDefaultColor);
}

const LocationBarModel* CustomTabBarView::GetLocationBarModel() const {
  return delegate_->GetLocationBarModel();
}

void CustomTabBarView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  GoBackToApp();
}

void CustomTabBarView::GoBackToAppForTesting() {
  GoBackToApp();
}

void CustomTabBarView::GoBackToApp() {
  content::WebContents* web_contents = GetWebContents();
  web_app::AppBrowserController* app_controller =
      chrome::FindBrowserWithWebContents(web_contents)->app_controller();
  content::NavigationController& controller = web_contents->GetController();

  content::NavigationEntry* entry = nullptr;
  int offset = 0;

  // Go back until we find an in scope url or run out of urls.
  while ((entry = controller.GetEntryAtOffset(offset)) &&
         !app_controller->IsUrlInAppScope(entry->GetURL())) {
    offset--;
  }

  // If there are no in scope urls, push the app's launch url and clear
  // the history.
  if (!entry) {
    content::NavigationController::LoadURLParams load(
        app_controller->GetAppLaunchURL());
    load.should_clear_history_list = true;
    controller.LoadURLWithParams(load);
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

bool CustomTabBarView::IsShowingOriginForTesting() const {
  return title_origin_view_ != nullptr &&
         title_origin_view_->IsShowingOriginForTesting();
}
