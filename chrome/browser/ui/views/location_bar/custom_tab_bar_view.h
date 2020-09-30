// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/web_apps/web_app_menu_button.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/context_menu_controller.h"

namespace gfx {
class Rect;
}

namespace views {
class FlexLayout;
class MenuRunner;
class ImageButton;
}

class BrowserView;
class CustomTabBarTitleOriginView;
class WebAppMenuButton;

// For Desktop PWAs, a CustomTabBarView displays a read only title and origin
// for the current page and a security status icon. This is visible if the
// hosted app window is displaying a page over HTTP or if the current page is
// outside of the app scope. For Android apps on ChromeOS, CustomTabBarView is
// additionally used to show a three-dot menu icon.
class CustomTabBarView : public views::AccessiblePaneView,
                         public TabStripModelObserver,
                         public ui::SimpleMenuModel::Delegate,
                         public views::ContextMenuController,
                         public IconLabelBubbleView::Delegate,
                         public LocationIconView::Delegate {
 public:
  static const char kViewClassName[];

  CustomTabBarView(BrowserView* browser_view,
                   LocationBarView::Delegate* delegate);
  ~CustomTabBarView() override;

  LocationIconView* location_icon_view() { return location_icon_view_; }
  AppMenuButton* custom_tab_menu_button() { return web_app_menu_button_; }

  // views::AccessiblePaneView:
  gfx::Rect GetAnchorBoundsInScreen() const override;
  const char* GetClassName() const override;
  void SetVisible(bool visible) override;
  gfx::Size CalculatePreferredSize() const override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnThemeChanged() override;

  // TabstripModelObserver:
  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override;

  // IconLabelBubbleView::Delegate:
  SkColor GetIconLabelBubbleSurroundingForegroundColor() const override;
  SkColor GetIconLabelBubbleBackgroundColor() const override;

  // LocationIconView::Delegate:
  content::WebContents* GetWebContents() override;
  bool IsEditingOrEmpty() const override;
  void OnLocationIconPressed(const ui::MouseEvent& event) override;
  void OnLocationIconDragged(const ui::MouseEvent& event) override;
  SkColor GetSecurityChipColor(
      security_state::SecurityLevel security_level) const override;
  bool ShowPageInfoDialog() override;
  const LocationBarModel* GetLocationBarModel() const override;
  ui::ImageModel GetLocationIcon(LocationIconView::Delegate::IconFetchedCallback
                                     on_icon_fetched) const override;

  // Methods for testing.
  base::string16 title_for_testing() const { return last_title_; }
  base::string16 location_for_testing() const { return last_location_; }
  views::ImageButton* close_button_for_testing() const { return close_button_; }
  ui::SimpleMenuModel* context_menu_for_testing() const {
    return context_menu_model_.get();
  }
  void GoBackToAppForTesting();
  bool IsShowingOriginForTesting() const;

 private:
  // Calculate the view's background and frame color from the current theme
  // provider.
  SkColor GetBackgroundColor() const;
  SkColor GetDefaultFrameColor() const;

  // Takes the web contents for the custom tab bar back to the app scope.
  void GoBackToApp();

  // Called when the AppInfo dialog closes to set the focus on the correct view
  // within the browser.
  void AppInfoClosedCallback(views::Widget::ClosedReason closed_reason,
                             bool reload_prompt);

  // views::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Get the app controller associated with the browser, if any.
  web_app::AppBrowserController* app_controller() const {
    return browser_->app_controller();
  }

  // Convenience method to return the theme color from |app_controller_|.
  base::Optional<SkColor> GetThemeColor() const;

  // Populates child elements with page details from the current WebContents.
  void UpdateContents();

  bool ShouldShowTitle() const;

  SkColor title_bar_color_;
  SkColor background_color_;

  base::string16 last_title_;
  base::string16 last_location_;

  views::ImageButton* close_button_ = nullptr;
  LocationBarView::Delegate* delegate_ = nullptr;
  LocationIconView* location_icon_view_ = nullptr;
  CustomTabBarTitleOriginView* title_origin_view_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  Browser* browser_ = nullptr;

  // This remains a nullptr for Desktop PWAs and is non-null for Android apps
  // on ChromeOS.
  WebAppMenuButton* web_app_menu_button_ = nullptr;

  views::FlexLayout* layout_manager_;

  base::WeakPtrFactory<CustomTabBarView> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CustomTabBarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_
