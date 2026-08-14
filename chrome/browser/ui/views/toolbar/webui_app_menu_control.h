// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_APP_MENU_CONTROL_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_APP_MENU_CONTROL_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/toolbar/app_menu_icon_controller.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class AccessiblePaneView;
}  // namespace views

class AppMenu;
class AppMenuButtonObserver;
class AppMenuModel;
class WebUIToolbarControlDelegate;

// A WebUI-based implementation of the App Menu control.
class WebUIAppMenuControl : public AppMenuControl {
 public:
  explicit WebUIAppMenuControl(WebUIToolbarControlDelegate& delegate);
  WebUIAppMenuControl(const WebUIAppMenuControl&) = delete;
  WebUIAppMenuControl& operator=(const WebUIAppMenuControl&) = delete;
  ~WebUIAppMenuControl() override;

  // AppMenuControl:
  views::BubbleAnchor GetAnchor() override;
  bool IsDrawn() const override;
  bool IsMenuShowing() const override;
  views::DialogDelegate* GetDialogDelegate() override;
  void CloseMenu() override;
  void ShowMenu() override;
  void AddObserver(AppMenuButtonObserver* observer) override;
  void RemoveObserver(AppMenuButtonObserver* observer) override;
  bool HasFocus() const override;
  void Focus(views::AccessiblePaneView* pane) override;
  void SetTypeAndSeverity(
      AppMenuIconController::TypeAndSeverity type_and_severity) override;
  void SetIsMaximizedOrFullscreen(bool maximized_or_fullscreen) override;
  views::View* GetFocusablePaneView() override;

  // Returns the current state of the app menu control.
  toolbar_ui_api::mojom::AppMenuControlStatePtr GetState() const;

  // Called by the WebUIToolbarWebView when the WebUI reports a focus change.
  void SetFocused(bool focused);

  // Handles the context menu for the app menu.
  void HandleContextMenu(const gfx::Rect& anchor_bounds,
                         ui::mojom::MenuSourceType source);

 private:
  // Updates the open state of the app menu control and notifies the delegate.
  void UpdateOpenState();

  const raw_ref<WebUIToolbarControlDelegate> delegate_;
  AppMenuIconController::TypeAndSeverity type_and_severity_{
      AppMenuIconController::IconType::kNone,
      AppMenuIconController::Severity::kNone};
  bool window_is_maximized_or_fullscreen_ = false;
  // Caches the focus state of the button within the WebUI.
  bool focused_ = false;
  base::ObserverList<AppMenuButtonObserver>::Unchecked observer_list_;
  std::unique_ptr<AppMenuModel> menu_model_;
  std::unique_ptr<AppMenu> menu_;

  base::WeakPtrFactory<WebUIAppMenuControl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_WEBUI_APP_MENU_CONTROL_H_
