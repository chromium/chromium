// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_menu_button.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(CustomTabBarView, views::AccessiblePaneView)

 public:
  CustomTabBarView(BrowserView* browser_view,
                   LocationBarView::Delegate* delegate);
  CustomTabBarView(const CustomTabBarView&) = delete;
  CustomTabBarView& operator=(const CustomTabBarView&) = delete;
  ~CustomTabBarView() override;

  LocationIconView* location_icon_view() { return location_icon_view_; }
  AppMenuButton* custom_tab_menu_button() { return web_app_menu_button_; }

  // views::AccessiblePaneView:
  gfx::Rect GetAnchorBoundsInScreen() const override;
  void SetVisible(bool visible) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
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
  std::optional<ui::ColorId> GetLocationIconBackgroundColorOverride()
      const override;

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
  std::u16string title_for_testing() const { return last_title_; }
  std::u16string location_for_testing() const { return last_location_; }
  views::ImageButton* close_button_for_testing() const { return close_button_; }
  ui::SimpleMenuModel* context_menu_for_testing() const {
    return context_menu_model_.get();
  }
  void GoBackToAppForTesting();
  bool IsShowingOriginForTesting() const;
  bool IsShowingCloseButtonForTesting() const;

 private:
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

  // Populates child elements with page details from the current WebContents.
  void UpdateContents();

  bool GetShowTitle() const;

  SkColor title_bar_color_ = SK_ColorTRANSPARENT;
  SkColor background_color_ = SK_ColorTRANSPARENT;

  std::u16string last_title_;
  std::u16string last_location_;

  raw_ptr<views::ImageButton> close_button_ = nullptr;
  raw_ptr<LocationBarView::Delegate> delegate_ = nullptr;
  raw_ptr<LocationIconView> location_icon_view_ = nullptr;
  raw_ptr<CustomTabBarTitleOriginView> title_origin_view_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> context_menu_model_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;
  raw_ptr<Browser> browser_ = nullptr;

  // This remains a nullptr for Desktop PWAs and is non-null for Android apps
  // on ChromeOS.
  raw_ptr<WebAppMenuButton> web_app_menu_button_ = nullptr;

  raw_ptr<views::FlexLayout> layout_manager_;

  base::WeakPtrFactory<CustomTabBarView> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CUSTOM_TAB_BAR_VIEW_H_
