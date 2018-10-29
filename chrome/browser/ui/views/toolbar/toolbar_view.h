// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/toolbar/back_forward_menu_model.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/upgrade_detector/upgrade_observer.h"
#include "components/prefs/pref_member.h"
#include "components/translate/core/browser/translate_step.h"
#include "components/translate/core/common/translate_errors.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/material_design/material_design_controller_observer.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/view.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/intent_helper/arc_navigation_throttle.h"
#include "chrome/browser/ui/views/intent_picker_bubble_view.h"
#include "components/arc/common/intent_helper.mojom.h"  // nogncheck https://crbug.com/784179
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#endif  // defined(OS_CHROMEOS)

class AppMenuButton;
class AvatarToolbarButton;
class BrowserAppMenuButton;
class Browser;
class HomeButton;
class ReloadButton;
class ToolbarButton;

namespace bookmarks {
class BookmarkBubbleObserver;
}

namespace media_router {
class CastToolbarButton;
}

// The Browser Window's toolbar.
class ToolbarView : public views::AccessiblePaneView,
                    public views::MenuButtonListener,
                    public ui::AcceleratorProvider,
                    public LocationBarView::Delegate,
                    public BrowserActionsContainer::Delegate,
                    public CommandObserver,
                    public views::ButtonListener,
                    public AppMenuIconController::Delegate,
                    public UpgradeObserver,
                    public ToolbarButtonProvider,
                    public BrowserRootView::DropTarget,
                    public ui::MaterialDesignControllerObserver {
 public:
  // The view class name.
  static const char kViewClassName[];

  explicit ToolbarView(Browser* browser, BrowserView* browser_view);
  ~ToolbarView() override;

  // Create the contents of the Browser Toolbar.
  void Init();

  // Forces the toolbar (and transitively the location bar) to update its
  // current state.  If |tab| is non-NULL, we're switching (back?) to this tab
  // and should restore any previous location bar state (such as user editing)
  // as well.
  void Update(content::WebContents* tab);

  // Clears the current state for |tab|.
  void ResetTabState(content::WebContents* tab);

  // Set focus to the toolbar with complete keyboard access, with the
  // focus initially set to the app menu. Focus will be restored
  // to the last focused view if the user escapes.
  void SetPaneFocusAndFocusAppMenu();

  // Returns true if the app menu is focused.
  bool IsAppMenuFocused();

#if defined(OS_CHROMEOS)
  void ShowIntentPickerBubble(
      std::vector<IntentPickerBubbleView::AppInfo> app_info,
      bool disable_stay_in_chrome,
      IntentPickerResponse callback);
#endif  // defined(OS_CHROMEOS)

  // Shows a bookmark bubble and anchors it appropriately.
  void ShowBookmarkBubble(const GURL& url,
                          bool already_bookmarked,
                          bookmarks::BookmarkBubbleObserver* observer);

  // Shows the translate bubble and anchors it appropriately.
  void ShowTranslateBubble(content::WebContents* web_contents,
                           translate::TranslateStep step,
                           translate::TranslateErrors::Type error_type,
                           bool is_user_gesture);

  // Accessors.
  Browser* browser() const { return browser_; }
  BrowserActionsContainer* browser_actions() const { return browser_actions_; }
  ToolbarButton* back_button() const { return back_; }
  ReloadButton* reload_button() const { return reload_; }
  LocationBarView* location_bar() const { return location_bar_; }
  media_router::CastToolbarButton* cast_button() const { return cast_; }
  AvatarToolbarButton* avatar_button() const { return avatar_; }
  BrowserAppMenuButton* app_menu_button() const { return app_menu_button_; }
  HomeButton* home_button() const { return home_; }
  AppMenuIconController* app_menu_icon_controller() {
    return &app_menu_icon_controller_;
  }

  // views::MenuButtonListener:
  void OnMenuButtonClicked(views::MenuButton* source,
                           const gfx::Point& point,
                           const ui::Event* event) override;

  // LocationBarView::Delegate:
  content::WebContents* GetWebContents() override;
  ToolbarModel* GetToolbarModel() override;
  const ToolbarModel* GetToolbarModel() const override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // BrowserActionsContainer::Delegate:
  views::MenuButton* GetOverflowReferenceView() override;
  base::Optional<int> GetMaxBrowserActionsWidth() const override;
  std::unique_ptr<ToolbarActionsBar> CreateToolbarActionsBar(
      ToolbarActionsBarDelegate* delegate,
      Browser* browser,
      ToolbarActionsBar* main_bar) const override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // UpgradeObserver toolbar_button_view_provider.
  void OnOutdatedInstall() override;
  void OnOutdatedInstallNoAutoUpdate() override;
  void OnCriticalUpgradeInstalled() override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;
  const char* GetClassName() const override;
  bool AcceleratorPressed(const ui::Accelerator& acc) override;
  void ChildPreferredSizeChanged(views::View* child) override;

 protected:
  // AccessiblePaneView:
  bool SetPaneFocusAndFocusDefault() override;

  // ui::MaterialDesignControllerObserver:
  void OnTouchUiChanged() override;

  bool is_display_mode_normal() const {
    return display_mode_ == DISPLAYMODE_NORMAL;
  }

 private:
  // Types of display mode this toolbar can have.
  enum DisplayMode {
    DISPLAYMODE_NORMAL,       // Normal toolbar with buttons, etc.
    DISPLAYMODE_LOCATION      // Slimline toolbar showing only compact location
                              // bar, used for popups.
  };

  // AppMenuIconController::Delegate:
  void UpdateSeverity(AppMenuIconController::IconType type,
                      AppMenuIconController::Severity severity,
                      bool animate) override;

  // ToolbarButtonProvider:
  BrowserActionsContainer* GetBrowserActionsContainer() override;
  PageActionIconContainerView* GetPageActionIconContainerView() override;
  AppMenuButton* GetAppMenuButton() override;
  gfx::Rect GetFindBarBoundingBox(int contents_height) const override;
  void FocusToolbar() override;
  views::AccessiblePaneView* GetAsAccessiblePaneView() override;

  // BrowserRootView::DropTarget
  BrowserRootView::DropIndex GetDropIndex(
      const ui::DropTargetEvent& event) override;
  views::View* GetViewForDrop() override;

  // Used to avoid duplicating the near-identical logic of
  // ToolbarView::CalculatePreferredSize() and ToolbarView::GetMinimumSize().
  // These two functions call through to GetSizeInternal(), passing themselves
  // as the function pointer |View::*get_size|.
  gfx::Size GetSizeInternal(gfx::Size (View::*get_size)() const) const;

  // Given toolbar contents of size |size|, returns the total toolbar size.
  gfx::Size SizeForContentSize(gfx::Size size) const;

  // Loads the images for all the child views.
  void LoadImages();

  // Shows the critical notification bubble against the app menu.
  void ShowCriticalNotification();

  // Shows the outdated install notification bubble against the app menu.
  // |auto_update_enabled| is set to true when auto-upate is on.
  void ShowOutdatedInstallNotification(bool auto_update_enabled);

  void OnShowHomeButtonChanged();

  // Controls. Most of these can be null, e.g. in popup windows. Only
  // |location_bar_| is guaranteed to exist. These pointers are owned by the
  // view hierarchy.
  ToolbarButton* back_ = nullptr;
  ToolbarButton* forward_ = nullptr;
  ReloadButton* reload_ = nullptr;
  HomeButton* home_ = nullptr;
  LocationBarView* location_bar_ = nullptr;
  BrowserActionsContainer* browser_actions_ = nullptr;
  media_router::CastToolbarButton* cast_ = nullptr;
  AvatarToolbarButton* avatar_ = nullptr;
  BrowserAppMenuButton* app_menu_button_ = nullptr;

  Browser* const browser_;
  BrowserView* const browser_view_;

  AppMenuIconController app_menu_icon_controller_;

  // Controls whether or not a home button should be shown on the toolbar.
  BooleanPrefMember show_home_button_;

  // The display mode used when laying out the toolbar.
  const DisplayMode display_mode_;

  ScopedObserver<ui::MaterialDesignController,
                 ui::MaterialDesignControllerObserver>
      md_observer_{this};

  // Whether this toolbar has been initialized.
  bool initialized_ = false;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ToolbarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_VIEW_H_
