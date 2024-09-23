// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_

#include <memory>

#include "ash/wm/window_state.h"
#include "ash/wm/window_state_observer.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views_aura.h"
#include "chrome/browser/ui/views/exclusive_access_bubble_views_context.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/display/display_observer.h"
#include "ui/views/context_menu_controller.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace gfx {
class ImageSkia;
class RoundedCornersF;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuRunner;
}

class ChromeNativeAppWindowViewsAuraAshBrowserTest;
class ExclusiveAccessBubbleViews;

// Ash-specific parts of ChromeNativeAppWindowViewsAura. This is used on CrOS.
class ChromeNativeAppWindowViewsAuraAsh
    : public ChromeNativeAppWindowViewsAura,
      public views::ContextMenuController,
      public display::DisplayObserver,
      public ui::AcceleratorProvider,
      public ExclusiveAccessContext,
      public ExclusiveAccessBubbleViewsContext,
      public ash::WindowStateObserver,
      public aura::WindowObserver {
 public:
  ChromeNativeAppWindowViewsAuraAsh();

  ChromeNativeAppWindowViewsAuraAsh(const ChromeNativeAppWindowViewsAuraAsh&) =
      delete;
  ChromeNativeAppWindowViewsAuraAsh& operator=(
      const ChromeNativeAppWindowViewsAuraAsh&) = delete;

  ~ChromeNativeAppWindowViewsAuraAsh() override;

 protected:
  // ChromeNativeAppWindowViewsAura:
  void InitializeWindow(
      extensions::AppWindow* app_window,
      const extensions::AppWindow::CreateParams& create_params) override;
  void OnBeforeWidgetInit(
      const extensions::AppWindow::CreateParams& create_params,
      views::Widget::InitParams* init_params,
      views::Widget* widget) override;
  std::unique_ptr<views::NonClientFrameView> CreateNonStandardAppFrame()
      override;
  bool ShouldRemoveStandardFrame() override;
  void EnsureAppIconCreated() override;
  gfx::RoundedCornersF GetWindowRadii() const override;

  // ui::BaseWindow:
  gfx::Rect GetRestoredBounds() const override;
  ui::mojom::WindowShowState GetRestoredState() const override;
  ui::ZOrderLevel GetZOrderLevel() const override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& p,
                                  ui::MenuSourceType source_type) override;

  // WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  views::ClientView* CreateClientView(views::Widget* widget) override;
  ui::ImageModel GetWindowIcon() override;

  // NativeAppWindow:
  void SetFullscreen(int fullscreen_types) override;
  void SetActivateOnPointer(bool activate_on_pointer) override;

  // display::DisplayObserver:
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // ui::AcceleratorProvider:
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;

  // ExclusiveAccessContext:
  Profile* GetProfile() override;
  bool IsFullscreen() const override;
  void EnterFullscreen(const GURL& url,
                       ExclusiveAccessBubbleType bubble_type,
                       int64_t display_id) override;
  void ExitFullscreen() override;
  void UpdateExclusiveAccessBubble(
      const ExclusiveAccessBubbleParams& params,
      ExclusiveAccessBubbleHideCallback first_hide_callback) override;
  bool IsExclusiveAccessBubbleDisplayed() const override;
  void OnExclusiveAccessUserInput() override;
  content::WebContents* GetWebContentsForExclusiveAccess() override;
  bool CanUserExitFullscreen() const override;

  // ExclusiveAccessBubbleViewsContext:
  ExclusiveAccessManager* GetExclusiveAccessManager() override;
  ui::AcceleratorProvider* GetAcceleratorProvider() override;
  gfx::NativeView GetBubbleParentView() const override;
  gfx::Rect GetClientAreaBoundsInScreen() const override;
  bool IsImmersiveModeEnabled() const override;
  gfx::Rect GetTopContainerBoundsInScreen() override;
  void DestroyAnyExclusiveAccessBubble() override;

  // WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // ash::WindowStateObserver:
  void OnPostWindowStateTypeChange(ash::WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           ImmersiveWorkFlow);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           ImmersiveModeFullscreenRestoreType);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           NoImmersiveModeWhenForcedFullscreen);
  FRIEND_TEST_ALL_PREFIXES(
      ChromeNativeAppWindowViewsAuraPublicSessionAshBrowserTest,
      PublicSessionNoImmersiveModeWhenFullscreen);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           RestoreImmersiveMode);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           NoImmersiveOrBubbleOutsidePublicSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(ChromeNativeAppWindowViewsAuraAshBrowserTest,
                           NoImmersiveOrBubbleOutsidePublicSessionDom);
  FRIEND_TEST_ALL_PREFIXES(
      ChromeNativeAppWindowViewsAuraPublicSessionAshBrowserTest,
      BubbleInsidePublicSessionWindow);
  FRIEND_TEST_ALL_PREFIXES(
      ChromeNativeAppWindowViewsAuraPublicSessionAshBrowserTest,
      BubbleInsidePublicSessionDom);
  FRIEND_TEST_ALL_PREFIXES(ShapedAppWindowTargeterTest,
                           ResizeInsetsWithinBounds);

  // Invoked to handle tablet mode change.
  void OnTabletModeToggled(bool enabled);

  // Callback for MenuRunner
  void OnMenuClosed();

  // Whether immersive mode should be enabled.
  bool ShouldEnableImmersiveMode() const;

  // Helper function to update the immersive mode based on the current
  // app's and window manager's state.
  void UpdateImmersiveMode();

  // Generates the standard custom icon
  gfx::Image GetCustomImage() override;
  // Generates the standard app icon
  gfx::Image GetAppIconImage() override;

  // Helper function to call AppServiceProxy to load icon.
  void LoadAppIcon(bool allow_placeholder_icon);
  // Invoked when the icon is loaded.
  void OnLoadIcon(apps::IconValuePtr icon_value);

  gfx::ImageSkia app_icon_image_skia_;

  // Used to show the system menu.
  std::unique_ptr<ui::MenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  // Used for displaying the toast with instructions on exiting fullscreen.
  std::unique_ptr<ExclusiveAccessManager> exclusive_access_manager_{
      std::make_unique<ExclusiveAccessManager>(this)};
  std::unique_ptr<ExclusiveAccessBubbleViews> exclusive_access_bubble_;

  bool tablet_mode_enabled_ = false;
  bool draggable_regions_sent_ = false;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      window_observation_{this};
  base::ScopedObservation<ash::WindowState, ash::WindowStateObserver>
      window_state_observation_{this};
  display::ScopedDisplayObserver display_observer_{this};

  base::WeakPtrFactory<ChromeNativeAppWindowViewsAuraAsh> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_AURA_ASH_H_
