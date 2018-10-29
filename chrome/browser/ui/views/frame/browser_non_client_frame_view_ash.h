// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_

#include <memory>

#include "ash/public/interfaces/ash_window_manager.mojom.h"
#include "ash/public/interfaces/split_view.mojom.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/command_observer.h"
#include "chrome/browser/ui/ash/tablet_mode_client_observer.h"
#include "chrome/browser/ui/views/frame/browser_frame_header_ash.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tab_icon_view_model.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/ws/common/types.h"
#include "ui/aura/window_observer.h"

class Browser;

namespace {
class HostedAppNonClientFrameViewAshTest;
}

class ProfileIndicatorIcon;
class TabIconView;

namespace ash {
class AshFrameCaptionController;
class DefaultFrameHeader;
class FrameCaptionButton;
class FrameCaptionButtonContainerView;
}  // namespace ash

// Provides the BrowserNonClientFrameView for Chrome OS.
class BrowserNonClientFrameViewAsh
    : public BrowserNonClientFrameView,
      public BrowserFrameHeaderAsh::AppearanceProvider,
      public TabletModeClientObserver,
      public TabIconViewModel,
      public CommandObserver,
      public ash::mojom::SplitViewObserver,
      public ash::FrameCaptionDelegate,
      public aura::WindowObserver,
      public ImmersiveModeController::Observer {
 public:
  BrowserNonClientFrameViewAsh(BrowserFrame* frame, BrowserView* browser_view);
  ~BrowserNonClientFrameViewAsh() override;

  void Init();

  ash::mojom::SplitViewObserverPtr CreateInterfacePtrForTesting();

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  void UpdateMinimumSize() override;
  void OnTabsMaxXChanged() override;
  bool CanUserExitFullscreen() const override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override;
  void ResetWindowControls() override;
  void UpdateWindowIcon() override;
  void UpdateWindowTitle() override;
  void SizeConstraintsChanged() override;
  void ActivationChanged(bool active) override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;
  const char* GetClassName() const override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  gfx::Size GetMinimumSize() const override;
  void OnThemeChanged() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // BrowserFrameHeaderAsh::AppearanceProvider:
  SkColor GetTitleColor() override;
  SkColor GetFrameHeaderColor(bool active) override;
  gfx::ImageSkia GetFrameHeaderImage(bool active) override;
  int GetFrameHeaderImageYInset() override;
  gfx::ImageSkia GetFrameHeaderOverlayImage(bool active) override;

  // TabletModeClientObserver:
  void OnTabletModeToggled(bool enabled) override;

  // TabIconViewModel:
  bool ShouldTabIconViewAnimate() const override;
  gfx::ImageSkia GetFaviconForTabIconView() override;

  // CommandObserver:
  void EnabledStateChangedForCommand(int id, bool enabled) override;

  // ash::mojom::SplitViewObserver:
  void OnSplitViewStateChanged(
      ash::mojom::SplitViewState current_state) override;

  // ash::FrameCaptionDelegate:
  bool CanSnap(aura::Window* window) override;
  void ShowSnapPreview(aura::Window* window,
                       ash::mojom::SnapDirection snap) override;
  void CommitSnap(aura::Window* window,
                  ash::mojom::SnapDirection snap) override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override;
  void OnImmersiveRevealEnded() override;
  void OnImmersiveFullscreenExited() override;

  // Returns true if the header should be painted so that it looks the same as
  // the header used for packaged apps.
  static bool UsePackagedAppHeaderStyle(const Browser* browser);

 protected:
  // BrowserNonClientFrameView:
  void OnProfileAvatarChanged(const base::FilePath& profile_path) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           NonImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ImmersiveFullscreen);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ToggleTabletModeRelayout);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           AvatarDisplayOnTeleportedWindow);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           HeaderVisibilityInOverviewAndSplitview);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ImmersiveModeTopViewInset);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshBackButtonTest,
                           V1BackButton);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           ToggleTabletModeOnMinimizedWindow);
  FRIEND_TEST_ALL_PREFIXES(HostedAppNonClientFrameViewAshTest,
                           ActiveStateOfButtonMatchesWidget);
  FRIEND_TEST_ALL_PREFIXES(BrowserNonClientFrameViewAshTest,
                           RestoreMinimizedBrowserUpdatesCaption);
  FRIEND_TEST_ALL_PREFIXES(ImmersiveModeControllerAshHostedAppBrowserTest,
                           FrameLayoutToggleTabletMode);
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherBrowserNonClientFrameViewAshTest,
                           TabletModeBrowserCaptionButtonVisibility);
  FRIEND_TEST_ALL_PREFIXES(HomeLauncherBrowserNonClientFrameViewAshTest,
                           TabletModeAppCaptionButtonVisibility);
  FRIEND_TEST_ALL_PREFIXES(NonHomeLauncherBrowserNonClientFrameViewAshTest,
                           HeaderHeightForSnappedBrowserInSplitView);

  friend class HostedAppNonClientFrameViewAshTest;

  // Returns whether the caption buttons should be visible. They are hidden, for
  // example, in overview mode and tablet mode.
  bool ShouldShowCaptionButtons() const;

  // Distance between the edges of the NonClientFrameView and the tab strip.
  int GetTabStripLeftInset() const;
  int GetTabStripRightInset() const;

  // Returns true if there is anything to paint. Some fullscreen windows do
  // not need their frames painted.
  bool ShouldPaint() const;

  // Helps to hide or show the header as needed when overview mode starts or
  // ends or when split view state changes.
  void OnOverviewOrSplitviewModeChanged();

  // Creates the frame header for the browser window.
  std::unique_ptr<ash::FrameHeader> CreateFrameHeader();

  // Creates views and does other setup for a hosted app.
  // TODO(estade): remove the parameter as it's unused in Mash.
  void SetUpForHostedApp(ash::DefaultFrameHeader* header);

  // Triggers the hosted app origin and icon animations, assumes the hosted
  // app UI elements exist.
  void StartHostedAppAnimation();

  // To be called after the frame's colors may have changed.
  void UpdateFrameColors();

  // Updates the kTopViewInset window property after a layout.
  void UpdateTopViewInset();

  // Returns true if |profile_indicator_icon_| should be shown.
  bool ShouldShowProfileIndicatorIcon() const;

  // Updates the icon that indicates a teleported window.
  void UpdateProfileIcons();

  void LayoutProfileIndicator();

  ws::Id GetServerWindowId() const;

  // Returns whether this window is currently in the overview list.
  bool IsInOverviewMode() const;

  void StartWindowMove(const ui::LocatedEvent& event);

  void OnWindowMoveDone(bool success);

  // Returns the top level aura::Window for this browser window.
  const aura::Window* GetFrameWindow() const;
  aura::Window* GetFrameWindow();

  // View which contains the window controls.
  ash::FrameCaptionButtonContainerView* caption_button_container_ = nullptr;

  ash::FrameCaptionButton* back_button_ = nullptr;

  // For popups, the window icon.
  TabIconView* window_icon_ = nullptr;

  // This is used for teleported windows (in multi-profile mode).
  ProfileIndicatorIcon* profile_indicator_icon_ = nullptr;

  // Helper class for painting the header.
  std::unique_ptr<ash::FrameHeader> frame_header_;

  // A helper for controlling the window frame; only used in !Mash.
  std::unique_ptr<ash::AshFrameCaptionController> caption_controller_;

  // Ash's mojom::SplitViewController.
  ash::mojom::SplitViewControllerPtr split_view_controller_;

  // The binding this instance uses to implement mojom::SplitViewObserver.
  mojo::Binding<ash::mojom::SplitViewObserver> observer_binding_{this};

  ScopedObserver<aura::Window, aura::WindowObserver> window_observer_{this};

  bool performing_window_move_ = false;

  // Maintains the current split view state.
  ash::mojom::SplitViewState split_view_state_ =
      ash::mojom::SplitViewState::NO_SNAP;

  // Only used in mash.
  ash::mojom::AshWindowManagerAssociatedPtr ash_window_manager_;

  base::WeakPtrFactory<BrowserNonClientFrameViewAsh> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAsh);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_NON_CLIENT_FRAME_VIEW_ASH_H_
