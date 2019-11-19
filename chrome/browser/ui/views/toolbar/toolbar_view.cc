// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/intent_picker_tab_helper.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_background.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_container_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/button_utils.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_account_icon_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/theme_provider.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/native_theme/native_theme_aura.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/recovery/recovery_install_global_error_factory.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/critical_notification_bubble_view.h"
#endif

#if !defined(OS_CHROMEOS)
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"
#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/views/frame/webui_tab_strip_container_view.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

using base::UserMetricsAction;
using content::WebContents;

namespace {

// Gets the display mode for a given browser.
ToolbarView::DisplayMode GetDisplayMode(Browser* browser) {
  // Checked in this order because even tabbed PWAs use the CUSTOM_TAB
  // display mode.
  if (web_app::AppBrowserController::IsForWebAppBrowser(browser))
    return ToolbarView::DisplayMode::CUSTOM_TAB;

  if (browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return ToolbarView::DisplayMode::NORMAL;

  return ToolbarView::DisplayMode::LOCATION;
}

}  // namespace

// static
const char ToolbarView::kViewClassName[] = "ToolbarView";

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

ToolbarView::ToolbarView(Browser* browser, BrowserView* browser_view)
    : AnimationDelegateViews(this),
      browser_(browser),
      browser_view_(browser_view),
      app_menu_icon_controller_(browser->profile(), this),
      display_mode_(GetDisplayMode(browser)) {
  SetID(VIEW_ID_TOOLBAR);

  chrome::AddCommandObserver(browser_, IDC_BACK, this);
  chrome::AddCommandObserver(browser_, IDC_FORWARD, this);
  chrome::AddCommandObserver(browser_, IDC_RELOAD, this);
  chrome::AddCommandObserver(browser_, IDC_HOME, this);
  chrome::AddCommandObserver(browser_, IDC_SHOW_AVATAR_MENU, this);

  UpgradeDetector::GetInstance()->AddObserver(this);
  md_observer_.Add(ui::MaterialDesignController::GetInstance());

  if (display_mode_ == DisplayMode::NORMAL)
    SetBackground(std::make_unique<TopContainerBackground>(browser_view));
}

ToolbarView::~ToolbarView() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);

  chrome::RemoveCommandObserver(browser_, IDC_BACK, this);
  chrome::RemoveCommandObserver(browser_, IDC_FORWARD, this);
  chrome::RemoveCommandObserver(browser_, IDC_RELOAD, this);
  chrome::RemoveCommandObserver(browser_, IDC_HOME, this);
  chrome::RemoveCommandObserver(browser_, IDC_SHOW_AVATAR_MENU, this);
}

void ToolbarView::Init() {
  auto location_bar = std::make_unique<LocationBarView>(
      browser_, browser_->profile(), browser_->command_controller(), this,
      display_mode_ != DisplayMode::NORMAL);
  // Make sure the toolbar shows by default.
  size_animation_.Reset(1);

  if (display_mode_ != DisplayMode::NORMAL) {
    location_bar_ = AddChildView(std::move(location_bar));
    location_bar_->Init();

    if (display_mode_ == DisplayMode::CUSTOM_TAB) {
      custom_tab_bar_ =
          AddChildView(std::make_unique<CustomTabBarView>(browser_view_, this));
    }

    SetLayoutManager(std::make_unique<views::FillLayout>());
    initialized_ = true;
    return;
  }

  std::unique_ptr<ToolbarButton> back = CreateBackButton(this, browser_);

  std::unique_ptr<ToolbarButton> forward = CreateForwardButton(this, browser_);

  std::unique_ptr<ReloadButton> reload = CreateReloadButton(browser_);

  std::unique_ptr<HomeButton> home = CreateHomeButton(this, browser_);

  std::unique_ptr<ExtensionsToolbarContainer> extensions_container;
  std::unique_ptr<BrowserActionsContainer> browser_actions;

  // Do not create the extensions or browser actions container if it is a guest
  // profile (only regular and incognito profiles host extensions).
  if (!browser_->profile()->IsGuestSession()) {
    if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
      extensions_container =
          std::make_unique<ExtensionsToolbarContainer>(browser_);
    } else {
      browser_actions =
          std::make_unique<BrowserActionsContainer>(browser_, nullptr, this);
    }
  }
  std::unique_ptr<media_router::CastToolbarButton> cast;
  if (media_router::MediaRouterEnabled(browser_->profile()))
    cast = media_router::CastToolbarButton::Create(browser_);

  std::unique_ptr<MediaToolbarButtonView> media_button;
  if (base::FeatureList::IsEnabled(media::kGlobalMediaControls)) {
    media_button = std::make_unique<MediaToolbarButtonView>(browser_);
  }

  std::unique_ptr<ToolbarAccountIconContainerView>
      toolbar_account_icon_container;
  bool show_avatar_toolbar_button = true;
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableToolbarStatusChip)) {
    // The avatar button is contained inside the page-action container and
    // should not be created twice.
    show_avatar_toolbar_button = false;
    toolbar_account_icon_container =
        std::make_unique<ToolbarAccountIconContainerView>(browser_);
  } else {
#if defined(OS_CHROMEOS)
    // ChromeOS only badges Incognito and Guest icons in the browser window.
    show_avatar_toolbar_button = browser_->profile()->IsOffTheRecord() ||
                                 browser_->profile()->IsGuestSession();
#endif
  }

  std::unique_ptr<AvatarToolbarButton> avatar;
  if (show_avatar_toolbar_button)
    avatar = std::make_unique<AvatarToolbarButton>(browser_);

  auto app_menu_button = std::make_unique<BrowserAppMenuButton>(this);
  app_menu_button->EnableCanvasFlippingForRTLUI(true);
  app_menu_button->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
  app_menu_button->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
  app_menu_button->SetID(VIEW_ID_APP_MENU);

  // Always add children in order from left to right, for accessibility.
  back_ = AddChildView(std::move(back));
  forward_ = AddChildView(std::move(forward));
  reload_ = AddChildView(std::move(reload));
  home_ = AddChildView(std::move(home));
  location_bar_ = AddChildView(std::move(location_bar));
  if (browser_actions)
    browser_actions_ = AddChildView(std::move(browser_actions));

  if (extensions_container)
    extensions_container_ = AddChildView(std::move(extensions_container));

  if (cast)
    cast_ = AddChildView(std::move(cast));

  if (media_button)
    media_button_ = AddChildView(std::move(media_button));

  if (toolbar_account_icon_container) {
    toolbar_account_icon_container_ =
        AddChildView(std::move(toolbar_account_icon_container));
  }

  // TODO(crbug.com/932818): Remove this once the
  // |kAutofillEnableToolbarStatusChip| is fully launched.
  if (avatar)
    avatar_ = AddChildView(std::move(avatar));

  app_menu_button_ = AddChildView(std::move(app_menu_button));

  LoadImages();

  // Start global error services now so we set the icon on the menu correctly.
#if !defined(OS_CHROMEOS)
  SigninGlobalErrorFactory::GetForProfile(browser_->profile());
#if defined(OS_WIN) || defined(OS_MACOSX)
  RecoveryInstallGlobalErrorFactory::GetForProfile(browser_->profile());
#endif
#endif  // OS_CHROMEOS

  // Set the button icon based on the system state. Do this after
  // |app_menu_button_| has been added as a bubble may be shown that needs
  // the widget (widget found by way of app_menu_button_->GetWidget()).
  app_menu_icon_controller_.UpdateDelegate();

  location_bar_->Init();

  show_home_button_.Init(
      prefs::kShowHomeButton, browser_->profile()->GetPrefs(),
      base::BindRepeating(&ToolbarView::OnShowHomeButtonChanged,
                          base::Unretained(this)));
  UpdateHomeButtonVisibility();

  InitLayout();

  initialized_ = true;
}

void ToolbarView::AnimationEnded(const gfx::Animation* animation) {
  if (animation->GetCurrentValue() == 0)
    SetToolbarVisibility(false);
  browser()->window()->ToolbarSizeChanged(/*is_animating=*/false);
}

void ToolbarView::AnimationProgressed(const gfx::Animation* animation) {
  browser()->window()->ToolbarSizeChanged(/*is_animating=*/true);
}

void ToolbarView::Update(WebContents* tab) {
  if (location_bar_)
    location_bar_->Update(tab);

  if (browser_actions_)
    browser_actions_->RefreshToolbarActionViews();

  if (extensions_container_)
    extensions_container_->UpdateAllIcons();

  if (reload_)
    reload_->set_menu_enabled(chrome::IsDebuggerAttachedToCurrentTab(browser_));

  if (toolbar_account_icon_container_)
    toolbar_account_icon_container_->UpdateAllIcons();
}

void ToolbarView::SetToolbarVisibility(bool visible) {
  SetVisible(visible);
  views::View* bar = display_mode_ == DisplayMode::CUSTOM_TAB
                         ? static_cast<views::View*>(custom_tab_bar_)
                         : static_cast<views::View*>(location_bar_);

  bar->SetVisible(visible);
}

void ToolbarView::UpdateCustomTabBarVisibility(bool visible, bool animate) {
  DCHECK_EQ(display_mode_, DisplayMode::CUSTOM_TAB);

  if (!animate) {
    size_animation_.Reset(visible ? 1.0 : 0.0);
    SetToolbarVisibility(visible);
    browser()->window()->ToolbarSizeChanged(/*is_animating=*/false);
    return;
  }

  if (visible) {
    SetToolbarVisibility(true);
    size_animation_.Show();
  } else {
    size_animation_.Hide();
  }
}

void ToolbarView::UpdateForWebUITabStrip() {
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (browser_view_->webui_tab_strip() && app_menu_button_) {
    const int insertion_index = GetIndexOf(app_menu_button_);
    AddChildViewAt(browser_view_->webui_tab_strip()->CreateTabCounter(),
                   insertion_index);
    AddChildViewAt(browser_view_->webui_tab_strip()->CreateNewTabButton(),
                   insertion_index);
    LoadImages();
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
}

void ToolbarView::ResetTabState(WebContents* tab) {
  if (location_bar_)
    location_bar_->ResetTabState(tab);
}

void ToolbarView::SetPaneFocusAndFocusAppMenu() {
  if (app_menu_button_)
    SetPaneFocus(app_menu_button_);
}

bool ToolbarView::IsAppMenuFocused() {
  return app_menu_button_ && app_menu_button_->HasFocus();
}

void ToolbarView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    PageActionIconType icon_type,
    const base::Optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  PageActionIconView* const intent_picker_view =
      GetPageActionIconView(icon_type);
  if (!intent_picker_view)
    return;

  IntentPickerBubbleView::ShowBubble(
      location_bar(), intent_picker_view, icon_type, GetWebContents(),
      std::move(app_info), show_stay_in_chrome, show_remember_selection,
      initiating_origin, std::move(callback));
  // TODO(knollr): find a way that the icon updates implicitly.
  intent_picker_view->Update();
}

void ToolbarView::ShowBookmarkBubble(
    const GURL& url,
    bool already_bookmarked,
    bookmarks::BookmarkBubbleObserver* observer) {
  views::View* const anchor_view = location_bar();
  PageActionIconView* const bookmark_star_icon =
      GetPageActionIconView(PageActionIconType::kBookmarkStar);

  std::unique_ptr<BubbleSyncPromoDelegate> delegate;
#if !defined(OS_CHROMEOS)
  // ChromeOS does not show the signin promo.
  delegate.reset(new BookmarkBubbleSignInDelegate(browser_));
#endif
  BookmarkBubbleView::ShowBubble(anchor_view, bookmark_star_icon, gfx::Rect(),
                                 nullptr, observer, std::move(delegate),
                                 browser_->profile(), url, already_bookmarked);
}

ExtensionsToolbarButton* ToolbarView::GetExtensionsButton() const {
  return extensions_container_->extensions_button();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

WebContents* ToolbarView::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

LocationBarModel* ToolbarView::GetLocationBarModel() {
  return browser_->location_bar_model();
}

const LocationBarModel* ToolbarView::GetLocationBarModel() const {
  return browser_->location_bar_model();
}

ContentSettingBubbleModelDelegate*
ToolbarView::GetContentSettingBubbleModelDelegate() {
  return browser_->content_setting_bubble_model_delegate();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, BrowserActionsContainer::Delegate implementation:

views::LabelButton* ToolbarView::GetOverflowReferenceView() {
  return app_menu_button_;
}

base::Optional<int> ToolbarView::GetMaxBrowserActionsWidth() const {
  // The browser actions container is allowed to grow, but only up until the
  // omnibox reaches its preferred size. So its maximum allowed width is its
  // current size, plus any that the omnibox could give up.
  return std::max(0, (browser_actions_ ? browser_actions_->width()
                                       : extensions_container_->width()) +
                         (location_bar_->width() -
                          location_bar_->GetPreferredSize().width()));
}

std::unique_ptr<ToolbarActionsBar> ToolbarView::CreateToolbarActionsBar(
    ToolbarActionsBarDelegate* delegate,
    Browser* browser,
    ToolbarActionsBar* main_bar) const {
  DCHECK_EQ(browser_, browser);
  return std::make_unique<ToolbarActionsBar>(delegate, browser, main_bar);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, CommandObserver implementation:

void ToolbarView::EnabledStateChangedForCommand(int id, bool enabled) {
  views::Button* button = nullptr;
  switch (id) {
    case IDC_BACK:
      button = back_;
      break;
    case IDC_FORWARD:
      button = forward_;
      break;
    case IDC_RELOAD:
      button = reload_;
      break;
    case IDC_HOME:
      button = home_;
      break;
    case IDC_SHOW_AVATAR_MENU:
      button = GetAvatarToolbarButton();
      break;
  }
  if (button)
    button->SetEnabled(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::Button::ButtonListener implementation:

void ToolbarView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
  TRACE_EVENT0("views", "ToolbarView::ButtonPressed");
  if (sender->GetID() == VIEW_ID_APP_MENU) {
    DCHECK_EQ(VIEW_ID_APP_MENU, sender->GetID());
    app_menu_button_->ShowMenu(event.IsKeyEvent()
                                   ? views::MenuRunner::SHOULD_SHOW_MNEMONICS
                                   : views::MenuRunner::NO_FLAGS);
  }

  chrome::ExecuteCommandWithDisposition(
      browser_, sender->tag(), ui::DispositionFromEventFlags(event.flags()));
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, UpgradeObserver implementation:
void ToolbarView::OnOutdatedInstall() {
  ShowOutdatedInstallNotification(true);
}

void ToolbarView::OnOutdatedInstallNoAutoUpdate() {
  ShowOutdatedInstallNotification(false);
}

void ToolbarView::OnCriticalUpgradeInstalled() {
  ShowCriticalNotification();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, ui::AcceleratorProvider implementation:

bool ToolbarView::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) const {
  return GetWidget()->GetAccelerator(command_id, accelerator);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::View overrides:

gfx::Size ToolbarView::CalculatePreferredSize() const {
  gfx::Size size = View::CalculatePreferredSize();
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

gfx::Size ToolbarView::GetMinimumSize() const {
  gfx::Size size = View::GetMinimumSize();
  size.set_height(size.height() * size_animation_.GetCurrentValue());
  return size;
}

void ToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (!initialized_)
    return;

  if (display_mode_ == DisplayMode::CUSTOM_TAB) {
    custom_tab_bar_->SetBounds(0, 0, width(),
                               custom_tab_bar_->GetPreferredSize().height());
    location_bar_->SetVisible(false);
    return;
  }

  if (display_mode_ == DisplayMode::LOCATION) {
    location_bar_->SetBounds(0, 0, width(),
                             location_bar_->GetPreferredSize().height());
    return;
  }

  LayoutCommon();

  // Call super implementation to ensure layout manager and child layouts
  // happen.
  AccessiblePaneView::Layout();
}

void ToolbarView::OnThemeChanged() {
  if (!initialized_)
    return;

  if (display_mode_ == DisplayMode::NORMAL)
    LoadImages();

  SchedulePaint();
}

const char* ToolbarView::GetClassName() const {
  return kViewClassName;
}

bool ToolbarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const views::View* focused_view = focus_manager()->GetFocusedView();
  if (focused_view && (focused_view->GetID() == VIEW_ID_OMNIBOX))
    return false;  // Let the omnibox handle all accelerator events.
  return AccessiblePaneView::AcceleratorPressed(accelerator);
}

void ToolbarView::ChildPreferredSizeChanged(views::View* child) {
  InvalidateLayout();
  if (size() != GetPreferredSize())
    PreferredSizeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, protected:

// Override this so that when the user presses F6 to rotate toolbar panes,
// the location bar gets focus, not the first control in the toolbar - and
// also so that it selects all content in the location bar.
bool ToolbarView::SetPaneFocusAndFocusDefault() {
  if (!location_bar_->HasFocus()) {
    SetPaneFocus(location_bar_);
    location_bar_->FocusLocation(true);
    return true;
  }

  if (!AccessiblePaneView::SetPaneFocusAndFocusDefault())
    return false;
  browser_->window()->RotatePaneFocus(true);
  return true;
}

// ui::MaterialDesignControllerObserver:
void ToolbarView::OnTouchUiChanged() {
  if (display_mode_ == DisplayMode::NORMAL) {
    // Update the internal margins for touch layout.
    // TODO(dfried): I think we can do better than this by making the touch UI
    // code cleaner.
    const int default_margin = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
    const int location_bar_margin = GetLayoutConstant(TOOLBAR_STANDARD_SPACING);
    layout_manager_->SetDefault(views::kMarginsKey,
                                gfx::Insets(0, default_margin));
    location_bar_->SetProperty(views::kMarginsKey,
                               gfx::Insets(0, location_bar_margin));
    if (browser_actions_) {
      browser_actions_->SetProperty(views::kInternalPaddingKey,
                                    gfx::Insets(0, location_bar_margin));
    }

    LoadImages();
    PreferredSizeChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

void ToolbarView::InitLayout() {
  const int default_margin = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  // TODO(dfried): rename this constant.
  const int location_bar_margin = GetLayoutConstant(TOOLBAR_STANDARD_SPACING);
  const views::FlexSpecification account_container_flex_rule =
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kScaleToMinimum,
          views::MaximumFlexSizeRule::kPreferred)
          .WithOrder(1);
  const views::FlexSpecification location_bar_flex_rule =
      views::FlexSpecification::ForSizeRule(
          views::MinimumFlexSizeRule::kScaleToMinimum,
          views::MaximumFlexSizeRule::kUnbounded)
          .WithOrder(2);
  const views::FlexSpecification browser_actions_flex_rule =
      views::FlexSpecification::ForCustomRule(
          BrowserActionsContainer::GetFlexRule())
          .WithOrder(3);

  layout_manager_ = SetLayoutManager(std::make_unique<views::FlexLayout>());

  layout_manager_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey, gfx::Insets(0, default_margin));

  location_bar_->SetProperty(views::kFlexBehaviorKey, location_bar_flex_rule);
  location_bar_->SetProperty(views::kMarginsKey,
                             gfx::Insets(0, location_bar_margin));

  if (browser_actions_) {
    browser_actions_->SetProperty(views::kFlexBehaviorKey,
                                  browser_actions_flex_rule);
    browser_actions_->SetProperty(views::kMarginsKey, gfx::Insets());
    browser_actions_->SetProperty(views::kInternalPaddingKey,
                                  gfx::Insets(0, location_bar_margin));
  }

  if (toolbar_account_icon_container_) {
    toolbar_account_icon_container_->SetProperty(views::kFlexBehaviorKey,
                                                 account_container_flex_rule);
  }

  LayoutCommon();
}

void ToolbarView::LayoutCommon() {
  DCHECK(display_mode_ == DisplayMode::NORMAL);

  const gfx::Insets interior_margin =
      GetLayoutInsets(LayoutInset::TOOLBAR_INTERIOR_MARGIN);
  layout_manager_->SetInteriorMargin(interior_margin);

  // Extend buttons to the window edge if we're either in a maximized or
  // fullscreen window. This makes the buttons easier to hit, see Fitts' law.
  const bool extend_buttons_to_edge =
      browser_->window() &&
      (browser_->window()->IsMaximized() || browser_->window()->IsFullscreen());
  back_->SetLeadingMargin(extend_buttons_to_edge ? interior_margin.left() : 0);
  app_menu_button_->SetTrailingMargin(
      extend_buttons_to_edge ? interior_margin.right() : 0);

  // Cast button visibility is controlled externally.
}

// AppMenuIconController::Delegate:
void ToolbarView::UpdateTypeAndSeverity(
    AppMenuIconController::TypeAndSeverity type_and_severity) {
  // There's no app menu in tabless windows.
  if (!app_menu_button_)
    return;

  base::string16 accname_app = l10n_util::GetStringUTF16(IDS_ACCNAME_APP);
  if (type_and_severity.type ==
      AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    accname_app = l10n_util::GetStringFUTF16(
        IDS_ACCNAME_APP_UPGRADE_RECOMMENDED, accname_app);
  }
  app_menu_button_->SetAccessibleName(accname_app);
  app_menu_button_->SetTypeAndSeverity(type_and_severity);
}

const ui::ThemeProvider* ToolbarView::GetViewThemeProvider() const {
  return GetThemeProvider();
}

ui::NativeTheme* ToolbarView::GetViewNativeTheme() {
  return GetNativeTheme();
}

// ToolbarButtonProvider:
BrowserActionsContainer* ToolbarView::GetBrowserActionsContainer() {
  CHECK(!base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu));
  return browser_actions_;
}

ToolbarActionView* ToolbarView::GetToolbarActionViewForId(
    const std::string& id) {
  if (display_mode_ != DisplayMode::NORMAL)
    return nullptr;
  if (base::FeatureList::IsEnabled(features::kExtensionsToolbarMenu)) {
    DCHECK(extensions_container_);
    return extensions_container_->GetViewForId(id);
  }
  DCHECK(GetBrowserActionsContainer());
  return GetBrowserActionsContainer()->GetViewForId(id);
}

views::View* ToolbarView::GetDefaultExtensionDialogAnchorView() {
  if (extensions_container_)
    return extensions_container_->extensions_button();
  return GetAppMenuButton();
}

PageActionIconView* ToolbarView::GetPageActionIconView(
    PageActionIconType type) {
  PageActionIconView* icon =
      location_bar()->page_action_icon_container()->GetIconView(type);
  if (icon)
    return icon;
  return toolbar_account_icon_container_
             ? toolbar_account_icon_container_->page_action_icon_container()
                   ->GetIconView(type)
             : nullptr;
}

AppMenuButton* ToolbarView::GetAppMenuButton() {
  return app_menu_button_;
}

gfx::Rect ToolbarView::GetFindBarBoundingBox(int contents_bottom) const {
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return gfx::Rect();

  if (!location_bar_->IsDrawn())
    return gfx::Rect();

  gfx::Rect bounds =
      location_bar_->ConvertRectToWidget(location_bar_->GetLocalBounds());
  return gfx::Rect(bounds.x(), bounds.bottom(), bounds.width(),
                   contents_bottom - bounds.bottom());
}

void ToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* ToolbarView::GetAsAccessiblePaneView() {
  return this;
}

views::View* ToolbarView::GetAnchorView(PageActionIconType type) {
  // Return the container visually housing the icon so all the bubbles align
  // with the same visible edge.
  if (toolbar_account_icon_container_) {
    views::View* icon = GetPageActionIconView(type);
    if (toolbar_account_icon_container_->Contains(icon)) {
      DCHECK(base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableToolbarStatusChip));
      return toolbar_account_icon_container_;
    }
  }
  return location_bar_;
}

void ToolbarView::ZoomChangedForActiveTab(bool can_show_bubble) {
  location_bar_->page_action_icon_container()->ZoomChangedForActiveTab(
      can_show_bubble);
}

AvatarToolbarButton* ToolbarView::GetAvatarToolbarButton() {
  if (toolbar_account_icon_container_ &&
      toolbar_account_icon_container_->avatar_button()) {
    return toolbar_account_icon_container_->avatar_button();
  }

  if (avatar_)
    return avatar_;

  return nullptr;
}

ToolbarButton* ToolbarView::GetBackButton() {
  return back_;
}

ReloadButton* ToolbarView::GetReloadButton() {
  return reload_;
}

BrowserRootView::DropIndex ToolbarView::GetDropIndex(
    const ui::DropTargetEvent& event) {
  return {browser_->tab_strip_model()->active_index(), false};
}

views::View* ToolbarView::GetViewForDrop() {
  return this;
}

void ToolbarView::LoadImages() {
  DCHECK_EQ(display_mode_, DisplayMode::NORMAL);

  const ui::ThemeProvider* tp = GetThemeProvider();

  const SkColor normal_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  const SkColor disabled_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE);

  if (browser_actions_) {
    browser_actions_->SetSeparatorColor(
        tp->GetColor(ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR));
  }

  const bool touch_ui = ui::MaterialDesignController::touch_ui();

  const gfx::VectorIcon& back_image =
      touch_ui ? kBackArrowTouchIcon : vector_icons::kBackArrowIcon;
  back_->SetImage(views::Button::STATE_NORMAL,
                  gfx::CreateVectorIcon(back_image, normal_color));
  back_->SetImage(views::Button::STATE_DISABLED,
                  gfx::CreateVectorIcon(back_image, disabled_color));

  const gfx::VectorIcon& forward_image =
      touch_ui ? kForwardArrowTouchIcon : vector_icons::kForwardArrowIcon;
  forward_->SetImage(views::Button::STATE_NORMAL,
                     gfx::CreateVectorIcon(forward_image, normal_color));
  forward_->SetImage(views::Button::STATE_DISABLED,
                     gfx::CreateVectorIcon(forward_image, disabled_color));

  const gfx::VectorIcon& home_image =
      touch_ui ? kNavigateHomeTouchIcon : kNavigateHomeIcon;
  home_->SetImage(views::Button::STATE_NORMAL,
                  gfx::CreateVectorIcon(home_image, normal_color));

  if (extensions_container_)
    extensions_container_->UpdateAllIcons();

  if (cast_)
    cast_->UpdateIcon();

  if (media_button_)
    media_button_->UpdateIcon();

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  if (browser_view_->webui_tab_strip())
    browser_view_->webui_tab_strip()->UpdateButtons();
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

  if (avatar_)
    avatar_->UpdateIcon();

  if (toolbar_account_icon_container_)
    toolbar_account_icon_container_->UpdateAllIcons();

  app_menu_button_->UpdateIcon();

  reload_->SetColors(normal_color, disabled_color);
}

void ToolbarView::ShowCriticalNotification() {
#if defined(OS_WIN)
  views::BubbleDialogDelegateView::CreateBubble(
      new CriticalNotificationBubbleView(app_menu_button_))->Show();
#endif
}

void ToolbarView::ShowOutdatedInstallNotification(bool auto_update_enabled) {
#if !defined(OS_CHROMEOS)
  OutdatedUpgradeBubbleView::ShowBubble(app_menu_button_, browser_,
                                        auto_update_enabled);
#endif
}

void ToolbarView::OnShowHomeButtonChanged() {
  UpdateHomeButtonVisibility();
  Layout();
  SchedulePaint();
}

void ToolbarView::UpdateHomeButtonVisibility() {
  const bool show_home_button =
      show_home_button_.GetValue() || browser_->deprecated_is_app();
  home_->SetVisible(show_home_button);
}
