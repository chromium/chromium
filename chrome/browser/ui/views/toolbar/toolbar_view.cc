// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include <algorithm>
#include <utility>

#include "base/command_line.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_bubble_sign_in_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_content_setting_bubble_model_delegate.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/browser/ui/views/media_router/cast_toolbar_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/home_button.h"
#include "chrome/browser/ui/views/toolbar/reload_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/browser/ui/views/translate/translate_bubble_view.h"
#include "chrome/browser/ui/views/translate/translate_icon_view.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
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
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/tooltip_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN) || defined(OS_MACOSX)
#include "chrome/browser/recovery/recovery_install_global_error_factory.h"
#endif

#if defined(OS_WIN)
#include "chrome/browser/ui/views/critical_notification_bubble_view.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/views/location_bar/intent_picker_view.h"
#else
#include "chrome/browser/signin/signin_global_error_factory.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
#include "chrome/browser/ui/views/outdated_upgrade_bubble_view.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

int GetToolbarHorizontalPadding() {
  // In the touch-optimized UI, we don't use any horizontal paddings; the back
  // button starts from the beginning of the view, and the app menu button ends
  // at the end of the view.
  return ui::MaterialDesignController::touch_ui() ? 0 : 8;
}

}  // namespace

// static
const char ToolbarView::kViewClassName[] = "ToolbarView";

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, public:

ToolbarView::ToolbarView(Browser* browser, BrowserView* browser_view)
    : browser_(browser),
      browser_view_(browser_view),
      app_menu_icon_controller_(browser->profile(), this),
      display_mode_(browser->SupportsWindowFeature(Browser::FEATURE_TABSTRIP)
                        ? DISPLAYMODE_NORMAL
                        : DISPLAYMODE_LOCATION) {
  set_id(VIEW_ID_TOOLBAR);

  chrome::AddCommandObserver(browser_, IDC_BACK, this);
  chrome::AddCommandObserver(browser_, IDC_FORWARD, this);
  chrome::AddCommandObserver(browser_, IDC_RELOAD, this);
  chrome::AddCommandObserver(browser_, IDC_HOME, this);
  chrome::AddCommandObserver(browser_, IDC_SHOW_AVATAR_MENU, this);
  chrome::AddCommandObserver(browser_, IDC_LOAD_NEW_TAB_PAGE, this);

  UpgradeDetector::GetInstance()->AddObserver(this);
  md_observer_.Add(ui::MaterialDesignController::GetInstance());
}

ToolbarView::~ToolbarView() {
  UpgradeDetector::GetInstance()->RemoveObserver(this);

  chrome::RemoveCommandObserver(browser_, IDC_BACK, this);
  chrome::RemoveCommandObserver(browser_, IDC_FORWARD, this);
  chrome::RemoveCommandObserver(browser_, IDC_RELOAD, this);
  chrome::RemoveCommandObserver(browser_, IDC_HOME, this);
  chrome::RemoveCommandObserver(browser_, IDC_SHOW_AVATAR_MENU, this);
  chrome::RemoveCommandObserver(browser_, IDC_LOAD_NEW_TAB_PAGE, this);
}

void ToolbarView::Init() {
  location_bar_ = new LocationBarView(browser_, browser_->profile(),
                                      browser_->command_controller(), this,
                                      !is_display_mode_normal());

  if (!is_display_mode_normal()) {
    AddChildView(location_bar_);
    location_bar_->Init();
    initialized_ = true;
    return;
  }

  back_ = new ToolbarButton(
      this,
      std::make_unique<BackForwardMenuModel>(
          browser_, BackForwardMenuModel::ModelType::kBackward),
      browser_->tab_strip_model());
  back_->set_hide_ink_drop_when_showing_context_menu(false);
  back_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON);
  back_->set_tag(IDC_BACK);
  back_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_BACK));
  back_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_BACK));
  back_->GetViewAccessibility().OverrideDescription(
      l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_BACK));
  back_->set_id(VIEW_ID_BACK_BUTTON);
  back_->Init();

  forward_ = new ToolbarButton(
      this,
      std::make_unique<BackForwardMenuModel>(
          browser_, BackForwardMenuModel::ModelType::kForward),
      browser_->tab_strip_model());
  forward_->set_hide_ink_drop_when_showing_context_menu(false);
  forward_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON);
  forward_->set_tag(IDC_FORWARD);
  forward_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_FORWARD));
  forward_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD));
  forward_->GetViewAccessibility().OverrideDescription(
      l10n_util::GetStringUTF8(IDS_ACCDESCRIPTION_FORWARD));
  forward_->set_id(VIEW_ID_FORWARD_BUTTON);
  forward_->Init();

  reload_ = new ReloadButton(browser_->command_controller());
  reload_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON);
  reload_->set_tag(IDC_RELOAD);
  reload_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_RELOAD));
  reload_->set_id(VIEW_ID_RELOAD_BUTTON);
  reload_->Init();

  home_ = new HomeButton(this, browser_);
  home_->set_triggerable_event_flags(
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_MIDDLE_MOUSE_BUTTON);
  home_->set_tag(IDC_HOME);
  home_->SetTooltipText(l10n_util::GetStringUTF16(IDS_TOOLTIP_HOME));
  home_->SetAccessibleName(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME));
  home_->set_id(VIEW_ID_HOME_BUTTON);
  home_->Init();

  // No master container for this one (it is master).
  BrowserActionsContainer* main_container = nullptr;
  browser_actions_ =
      new BrowserActionsContainer(browser_, main_container, this);

  if (media_router::MediaRouterEnabled(browser_->profile()) &&
      media_router::ShouldUseViewsDialog()) {
    cast_ = media_router::CastToolbarButton::Create(browser_).release();
  }

  bool show_avatar_toolbar_button = true;
#if defined(OS_CHROMEOS)
  // ChromeOS only badges Incognito and Guest icons in the browser window.
  show_avatar_toolbar_button = browser_->profile()->IsOffTheRecord() ||
                               browser_->profile()->IsGuestSession();
#endif  // !defined(OS_CHROMEOS)
  if (show_avatar_toolbar_button)
    avatar_ = new AvatarToolbarButton(browser_);

  app_menu_button_ = new BrowserAppMenuButton(this);
  app_menu_button_->EnableCanvasFlippingForRTLUI(true);
  app_menu_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_APP));
  app_menu_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_APPMENU_TOOLTIP));
  app_menu_button_->set_id(VIEW_ID_APP_MENU);

  // Always add children in order from left to right, for accessibility.
  AddChildView(back_);
  AddChildView(forward_);
  AddChildView(reload_);
  AddChildView(home_);
  AddChildView(location_bar_);
  AddChildView(browser_actions_);
  if (cast_)
    AddChildView(cast_);
  if (avatar_)
    AddChildView(avatar_);
  AddChildView(app_menu_button_);

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

  initialized_ = true;
}

void ToolbarView::Update(WebContents* tab) {
  if (location_bar_)
    location_bar_->Update(tab);
  if (browser_actions_)
    browser_actions_->RefreshToolbarActionViews();
  if (reload_)
    reload_->set_menu_enabled(chrome::IsDebuggerAttachedToCurrentTab(browser_));
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

#if defined(OS_CHROMEOS)
void ToolbarView::ShowIntentPickerBubble(
    std::vector<IntentPickerBubbleView::AppInfo> app_info,
    bool disable_stay_in_chrome,
    IntentPickerResponse callback) {
  IntentPickerView* intent_picker_view = location_bar()->intent_picker_view();
  if (intent_picker_view) {
    if (!intent_picker_view->visible()) {
      intent_picker_view->SetVisible(true);
      location_bar()->Layout();
    }

    IntentPickerBubbleView::ShowBubble(
        intent_picker_view, GetWebContents(), std::move(app_info),
        disable_stay_in_chrome, std::move(callback));
  }
}
#endif  // defined(OS_CHROMEOS)

void ToolbarView::ShowBookmarkBubble(
    const GURL& url,
    bool already_bookmarked,
    bookmarks::BookmarkBubbleObserver* observer) {
  views::View* const anchor_view = location_bar();
  PageActionIconView* const star_view = location_bar()->star_view();

  std::unique_ptr<BubbleSyncPromoDelegate> delegate;
  delegate.reset(new BookmarkBubbleSignInDelegate(browser_));
  BookmarkBubbleView::ShowBubble(anchor_view, star_view, gfx::Rect(), nullptr,
                                 observer, std::move(delegate),
                                 browser_->profile(), url, already_bookmarked);
}

void ToolbarView::ShowTranslateBubble(
    content::WebContents* web_contents,
    translate::TranslateStep step,
    translate::TranslateErrors::Type error_type,
    bool is_user_gesture) {
  views::View* const anchor_view = location_bar();
  PageActionIconView* translate_icon_view =
      location_bar()->translate_icon_view();

  TranslateBubbleView::ShowBubble(anchor_view, translate_icon_view,
                                  gfx::Point(), web_contents, step, error_type,
                                  is_user_gesture
                                      ? TranslateBubbleView::USER_GESTURE
                                      : TranslateBubbleView::AUTOMATIC);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::MenuButtonListener implementation:

void ToolbarView::OnMenuButtonClicked(views::MenuButton* source,
                                      const gfx::Point& point,
                                      const ui::Event* event) {
  TRACE_EVENT0("views", "ToolbarView::OnMenuButtonClicked");
  DCHECK_EQ(VIEW_ID_APP_MENU, source->id());
  app_menu_button_->ShowMenu(false);  // Not for drop.
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, LocationBarView::Delegate implementation:

WebContents* ToolbarView::GetWebContents() {
  return browser_->tab_strip_model()->GetActiveWebContents();
}

ToolbarModel* ToolbarView::GetToolbarModel() {
  return browser_->toolbar_model();
}

const ToolbarModel* ToolbarView::GetToolbarModel() const {
  return browser_->toolbar_model();
}

ContentSettingBubbleModelDelegate*
ToolbarView::GetContentSettingBubbleModelDelegate() {
  return browser_->content_setting_bubble_model_delegate();
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, BrowserActionsContainer::Delegate implementation:

views::MenuButton* ToolbarView::GetOverflowReferenceView() {
  return app_menu_button_;
}

base::Optional<int> ToolbarView::GetMaxBrowserActionsWidth() const {
  // The browser actions container is allowed to grow, but only up until the
  // omnibox reaches its minimum size. So its maximum allowed width is its
  // current size, plus any that the omnibox could give up.
  return browser_actions_->width() +
         (location_bar_->width() - location_bar_->GetMinimumSize().width());
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
      button = avatar_;
      break;
  }
  if (button)
    button->SetEnabled(enabled);
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, views::Button::ButtonListener implementation:

void ToolbarView::ButtonPressed(views::Button* sender,
                                const ui::Event& event) {
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
  return GetSizeInternal(&View::GetPreferredSize);
}

gfx::Size ToolbarView::GetMinimumSize() const {
  return GetSizeInternal(&View::GetMinimumSize);
}

void ToolbarView::Layout() {
  // If we have not been initialized yet just do nothing.
  if (!initialized_)
    return;

  if (!is_display_mode_normal()) {
    location_bar_->SetBounds(0, 0, width(),
                             location_bar_->GetPreferredSize().height());
    return;
  }

  // We assume all toolbar buttons except for the browser actions are the same
  // height. Set toolbar_button_y such that buttons appear vertically centered.
  const int toolbar_button_height =
      std::min(back_->GetPreferredSize().height(), height());
  const int toolbar_button_y = (height() - toolbar_button_height) / 2;

  // If the window is maximized, we extend the back button to the left so that
  // clicking on the left-most pixel will activate the back button.
  // TODO(abarth):  If the window becomes maximized but is not resized,
  //                then Layout() might not be called and the back button
  //                will be slightly the wrong size.  We should force a
  //                Layout() in this case.
  //                http://crbug.com/5540
  const bool maximized =
      browser_->window() && browser_->window()->IsMaximized();
  // The padding at either end of the toolbar.
  const int end_padding = GetToolbarHorizontalPadding();
  back_->SetLeadingMargin(maximized ? end_padding : 0);
  back_->SetBounds(maximized ? 0 : end_padding, toolbar_button_y,
                   back_->GetPreferredSize().width(), toolbar_button_height);
  const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
  int next_element_x = back_->bounds().right() + element_padding;

  forward_->SetBounds(next_element_x, toolbar_button_y,
                      forward_->GetPreferredSize().width(),
                      toolbar_button_height);
  next_element_x = forward_->bounds().right() + element_padding;

  reload_->SetBounds(next_element_x, toolbar_button_y,
                     reload_->GetPreferredSize().width(),
                     toolbar_button_height);
  next_element_x = reload_->bounds().right();

  home_->SetSize(
      gfx::Size(home_->GetPreferredSize().width(), toolbar_button_height));
  if (show_home_button_.GetValue() ||
      (browser_->is_app() && extensions::util::IsNewBookmarkAppsEnabled())) {
    home_->SetVisible(true);
    next_element_x += element_padding;
    home_->SetPosition(gfx::Point(next_element_x, toolbar_button_y));
    next_element_x += home_->width();
  } else {
    home_->SetVisible(false);
  }

  next_element_x += GetLayoutConstant(TOOLBAR_STANDARD_SPACING);

  int app_menu_width = app_menu_button_->GetPreferredSize().width();
  const int right_padding = GetLayoutConstant(TOOLBAR_STANDARD_SPACING);

  // Note that the browser actions container has its own internal left and right
  // padding to visually separate it from the location bar and app menu button.
  // However if the container is empty we must account for the |right_padding|
  // value used to visually separate the location bar and app menu button.
  int available_width = std::max(
      0,
      width() - end_padding - app_menu_width -
      (browser_actions_->GetPreferredSize().IsEmpty() ? right_padding : 0) -
      next_element_x);
  if (cast_ && cast_->visible()) {
    available_width -= cast_->GetPreferredSize().width();
    available_width -= element_padding;
  }
  if (avatar_) {
    available_width -= avatar_->GetPreferredSize().width();
    available_width -= element_padding;
  }
  // Don't allow the omnibox to shrink to the point of non-existence, so
  // subtract its minimum width from the available width to reserve it.
  const int browser_actions_width = browser_actions_->GetWidthForMaxWidth(
      available_width - location_bar_->GetMinimumSize().width());
  available_width -= browser_actions_width;
  const int location_bar_width = available_width;

  const int location_height = location_bar_->GetPreferredSize().height();
  const int location_y = (height() - location_height) / 2;
  location_bar_->SetBounds(next_element_x, location_y,
                           location_bar_width, location_height);
  next_element_x = location_bar_->bounds().right();

  // Note height() may be zero in fullscreen.
  const int browser_actions_height =
      std::min(browser_actions_->GetPreferredSize().height(), height());
  const int browser_actions_y = (height() - browser_actions_height) / 2;
  browser_actions_->SetBounds(next_element_x, browser_actions_y,
                              browser_actions_width, browser_actions_height);
  next_element_x = browser_actions_->bounds().right();
  if (!browser_actions_width)
    next_element_x += right_padding;

  // The browser actions need to do a layout explicitly, because when an
  // extension is loaded/unloaded/changed, BrowserActionContainer removes and
  // re-adds everything, regardless of whether it has a page action. For a
  // page action, browser action bounds do not change, as a result of which
  // SetBounds does not do a layout at all.
  // TODO(sidchat): Rework the above behavior so that explicit layout is not
  //                required.
  browser_actions_->Layout();

  if (cast_ && cast_->visible()) {
    cast_->SetBounds(next_element_x, toolbar_button_y,
                     cast_->GetPreferredSize().width(), toolbar_button_height);
    next_element_x = cast_->bounds().right() + element_padding;
  }
  if (avatar_) {
    avatar_->SetBounds(next_element_x, toolbar_button_y,
                       avatar_->GetPreferredSize().width(),
                       toolbar_button_height);
    next_element_x = avatar_->bounds().right() + element_padding;
  }

  // Extend the app menu to the screen's right edge in maximized mode just like
  // we extend the back button to the left edge.
  if (maximized)
    app_menu_width += end_padding;

  // Set trailing margin before updating the bounds so OnBoundsChange can use
  // the trailing margin.
  app_menu_button_->SetTrailingMargin(maximized ? end_padding : 0);
  app_menu_button_->SetBounds(next_element_x, toolbar_button_y, app_menu_width,
                              toolbar_button_height);
}

void ToolbarView::OnPaintBackground(gfx::Canvas* canvas) {
  if (!is_display_mode_normal())
    return;

  const ui::ThemeProvider* tp = GetThemeProvider();

  // Always fill the toolbar with its bg color first in case the image is
  // transparent.
  canvas->FillRect(GetLocalBounds(),
                   tp->GetColor(ThemeProperties::COLOR_TOOLBAR));

  if (tp->HasCustomImage(IDR_THEME_TOOLBAR)) {
    const int x_offset =
        GetMirroredX() + browser_view_->GetMirroredX() +
        browser_view_->frame()->GetFrameView()->GetThemeBackgroundXInset();
    const int y_offset = GetLayoutConstant(TAB_HEIGHT) -
                         browser_view_->tabstrip()->GetStrokeThickness() -
                         GetLayoutConstant(TABSTRIP_TOOLBAR_OVERLAP);
    canvas->TileImageInt(*tp->GetImageSkiaNamed(IDR_THEME_TOOLBAR), x_offset,
                         y_offset, 0, 0, width(), height());
  }

  // Toolbar/content separator.
  BrowserView::Paint1pxHorizontalLine(
      canvas,
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_CONTENT_AREA_SEPARATOR),
      GetLocalBounds(), true);
}

void ToolbarView::OnThemeChanged() {
  if (is_display_mode_normal())
    LoadImages();
}

const char* ToolbarView::GetClassName() const {
  return kViewClassName;
}

bool ToolbarView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  const views::View* focused_view = focus_manager()->GetFocusedView();
  if (focused_view && (focused_view->id() == VIEW_ID_OMNIBOX))
    return false;  // Let the omnibox handle all accelerator events.
  return AccessiblePaneView::AcceleratorPressed(accelerator);
}

void ToolbarView::ChildPreferredSizeChanged(views::View* child) {
  Layout();
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
  if (is_display_mode_normal()) {
    LoadImages();
    PreferredSizeChanged();
  }
}

////////////////////////////////////////////////////////////////////////////////
// ToolbarView, private:

// AppMenuIconController::Delegate:
void ToolbarView::UpdateSeverity(AppMenuIconController::IconType type,
                                 AppMenuIconController::Severity severity,
                                 bool animate) {
  // There's no app menu in tabless windows.
  if (!app_menu_button_)
    return;

  base::string16 accname_app = l10n_util::GetStringUTF16(IDS_ACCNAME_APP);
  if (type == AppMenuIconController::IconType::UPGRADE_NOTIFICATION) {
    accname_app = l10n_util::GetStringFUTF16(
        IDS_ACCNAME_APP_UPGRADE_RECOMMENDED, accname_app);
  }
  app_menu_button_->SetAccessibleName(accname_app);
  app_menu_button_->SetSeverity(type, severity, animate);
}

// ToolbarButtonProvider:
BrowserActionsContainer* ToolbarView::GetBrowserActionsContainer() {
  return browser_actions_;
}

PageActionIconContainerView* ToolbarView::GetPageActionIconContainerView() {
  return location_bar_->page_action_icon_container_view();
}

AppMenuButton* ToolbarView::GetAppMenuButton() {
  return app_menu_button_;
}

gfx::Rect ToolbarView::GetFindBarBoundingBox(int contents_height) const {
  if (!browser_->SupportsWindowFeature(Browser::FEATURE_LOCATIONBAR))
    return gfx::Rect();

  if (!location_bar_->IsDrawn())
    return gfx::Rect();

  gfx::Rect bounds =
      location_bar_->ConvertRectToWidget(location_bar_->GetLocalBounds());
  return gfx::Rect(bounds.x(), bounds.bottom(), bounds.width(),
                   contents_height);
}

void ToolbarView::FocusToolbar() {
  SetPaneFocus(nullptr);
}

views::AccessiblePaneView* ToolbarView::GetAsAccessiblePaneView() {
  return this;
}

BrowserRootView::DropIndex ToolbarView::GetDropIndex(
    const ui::DropTargetEvent& event) {
  return {browser_->tab_strip_model()->active_index(), false};
}

views::View* ToolbarView::GetViewForDrop() {
  return this;
}

gfx::Size ToolbarView::GetSizeInternal(
    gfx::Size (View::*get_size)() const) const {
  gfx::Size size((location_bar_->*get_size)());
  if (is_display_mode_normal()) {
    const int element_padding = GetLayoutConstant(TOOLBAR_ELEMENT_PADDING);
    const int browser_actions_width =
        (browser_actions_->*get_size)().width();
    const int content_width =
        2 * GetToolbarHorizontalPadding() + (back_->*get_size)().width() +
        element_padding + (forward_->*get_size)().width() + element_padding +
        (reload_->*get_size)().width() +
        (show_home_button_.GetValue()
             ? element_padding + (home_->*get_size)().width()
             : 0) +
        GetLayoutConstant(TOOLBAR_STANDARD_SPACING) +
        (browser_actions_width > 0
             ? browser_actions_width
             : GetLayoutConstant(TOOLBAR_STANDARD_SPACING)) +
        (app_menu_button_->*get_size)().width();
    size.Enlarge(content_width, 0);
  }
  return SizeForContentSize(size);
}

gfx::Size ToolbarView::SizeForContentSize(gfx::Size size) const {
  if (is_display_mode_normal()) {
    // The size of the toolbar is computed using the size of the location bar
    // and constant padding values.
    int content_height = std::max(back_->GetPreferredSize().height(),
                                  location_bar_->GetPreferredSize().height());
    // In the touch-optimized UI, the toolbar buttons are big and occupy the
    // entire view's height, we don't need to add any extra vertical space.
    const int extra_vertical_space =
        ui::MaterialDesignController::touch_ui() ? 0 : 9;
    size.SetToMax(gfx::Size(0, content_height + extra_vertical_space));
  }
  return size;
}

void ToolbarView::LoadImages() {
  DCHECK(is_display_mode_normal());

  const ui::ThemeProvider* tp = GetThemeProvider();

  const SkColor normal_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON);
  const SkColor disabled_color =
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_BUTTON_ICON_INACTIVE);

  browser_actions_->SetSeparatorColor(
      tp->GetColor(ThemeProperties::COLOR_TOOLBAR_VERTICAL_SEPARATOR));

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

  if (cast_)
    cast_->UpdateIcon();
  if (avatar_)
    avatar_->UpdateIcon();

  app_menu_button_->UpdateIcon(false);

  reload_->LoadImages();
}

void ToolbarView::ShowCriticalNotification() {
#if defined(OS_WIN)
  views::BubbleDialogDelegateView::CreateBubble(
      new CriticalNotificationBubbleView(app_menu_button_))->Show();
#endif
}

void ToolbarView::ShowOutdatedInstallNotification(bool auto_update_enabled) {
#if !defined(OS_CHROMEOS) && !defined(OS_MACOSX)
  // TODO(tapted): Show this on Mac. See http://crbug.com/764111.
  OutdatedUpgradeBubbleView::ShowBubble(app_menu_button_, browser_,
                                        auto_update_enabled);
#endif
}

void ToolbarView::OnShowHomeButtonChanged() {
  Layout();
  SchedulePaint();
}
