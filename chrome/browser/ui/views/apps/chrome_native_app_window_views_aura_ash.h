// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_

#include <memory>
#include <vector>

#include "ash/public/interfaces/ash_window_manager.mojom.h"
#include "ash/public/interfaces/menu.mojom.h"
#include "ash/wm/window_state_observer.h"  // mash-ok
#include "base/gtest_prod_util.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/aura/window_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/views/context_menu_controller.h"

namespace ui {
class MenuModel;
}

namespace views {
class MenuRunner;
}

class ChromeNativeAppWindowViewsAuraAshBrowserTest;
class ExclusiveAccessBubbleViews;
class ExclusiveAccessManager;

// Ash-specific parts of ChromeNativeAppWindowViewsAura. This is used on CrOS.
class ChromeNativeAppWindowViewsAuraAsh
    : public ChromeNativeAppWindowViewsAura,
      public views::ContextMenuController,
      public TabletModeClientObserver,
      public ui::AcceleratorProvider,
      public ExclusiveAccessContext,
      public ExclusiveAccessBubbleViewsContext,
      public ash::wm::WindowStateObserver,
      public aura::WindowObserver,
      public MultiUserWindowManager::Observer,
      public ash::mojom::MenuDelegate {
 public:
  ChromeNativeAppWindowViewsAuraAsh();
  ~ChromeNativeAppWindowViewsAuraAsh() override;

 protected:
  // NativeAppWindowViews:
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;

  // ChromeNativeAppWindowViews:
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  views::NonClientFrameView* CreateNonStandardAppFrame() override;
  bool ShouldRemoveStandardFrame() override;

  // ui::BaseWindow:
  gfx::Rect GetRestoredBounds() const override;
  ui::WindowShowState GetRestoredState() const override;
  bool IsAlwaysOnTop() const override;

  // views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& p,
                              ui::MenuSourceType source_type) override;

  // WidgetDelegate:
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  ui::ModalType GetModalType() const override;

  // NativeAppWindow:
  void SetFullscreen(int fullscreen_types) override;
  void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) override;
  void SetActivateOnPointer(bool activate_on_pointer) override;

  // ash:TabletModeObserver:
  void OnTabletModeToggled(bool enabled) override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  bool IsFullscreen() const override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessExitBubbleContent(
      const GURL& url,
      ExclusiveAccessBubbleType bubble_type,
      ExclusiveAccessBubbleHideCallback bubble_first_hide_callback,
      bool force_update) override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetActiveWebContents() override;
  void UnhideDownloadShelf() override;
  void HideDownloadShelf() override;
  bool CanUserExitFullscreen() const override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  views::Widget* GetBubbleAssociatedWidget() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Point GetCursorPointInParent() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() const override;
  gfx::Rect GetTopContainerBoundsInScreen() override;
  void DestroyAnyExclusiveAccessBubble() override;
  bool CanTriggerOnMouse() const override;

  // WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ash::wm::WindowStateObserver
  void OnPostWindowStateTypeChange(
      ash::wm::WindowState* window_state,
      ash::mojom::WindowStateType old_type) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // MultiUserWindowManager::Observer:
  void OnOwnerEntryAdded(aura::Window* window) override;
  void OnOwnerEntryChanged(aura::Window* window) override;

  // ash::mojom::MenuDelegate:
  void MenuItemActivated(int command_id) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           ImmersiveWorkFlow);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           ImmersiveModeFullscreenRestoreType);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           NoImmersiveModeWhenForcedFullscreen);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           PublicSessionImmersiveMode);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           RestoreImmersiveMode);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshInteractiveTest,
                           NoImmersiveOrBubbleOutsidePublicSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshInteractiveTest,
                           NoImmersiveOrBubbleOutsidePublicSessionDom);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshInteractiveTest,
                           ImmersiveAndBubbleInsidePublicSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshInteractiveTest,
                           ImmersiveAndBubbleInsidePublicSessionDom);
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  // Callback for MenuRunner
  void OnMenuClosed();

  // Callback for Ash-controlled context menus, invoked over Mojo.
  void ExecuteContextMenuItem(int command_id);

  // Whether immersive mode should be enabled.
  bool ShouldEnableImmersiveMode() const;

  // Helper function to update the immersive mode based on the current
  // app's and window manager's state.
  void UpdateImmersiveMode();

  // Used to show the system menu. Only used in !Mash.
  std::unique_ptr<ui::MenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Used for displaying the toast with instructions on exiting fullscreen.
  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_;
  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

  bool tablet_mode_enabled_ = false;

  // Only used in mash.
  ash::mojom::AshWindowManagerAssociatedPtr ash_window_manager_;
  mojo::Binding<ash::mojom::MenuDelegate> binding_{this};

  ScopedObserver<aura::Window, aura::WindowObserver> observed_window_{this};
  ScopedObserver<ash::wm::WindowState, ash::wm::WindowStateObserver>
      observed_window_state_{this};

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsAuraAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_
