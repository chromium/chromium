// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_MAC_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/cocoa/immersive_mode_reveal_client.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

std::unique_ptr<ImmersiveModeController> CreateImmersiveModeControllerMac(
    const BrowserView* browser_view);

class ImmersiveModeControllerMac;

// This class notifies the browser view to refresh layout whenever the overlay
// widget moves. This is necessary for positioning web dialogs.
class ImmersiveModeOverlayWidgetObserver : public views::WidgetObserver {
 public:
  explicit ImmersiveModeOverlayWidgetObserver(
      ImmersiveModeControllerMac* controller);

  ImmersiveModeOverlayWidgetObserver(
      const ImmersiveModeOverlayWidgetObserver&) = delete;
  ImmersiveModeOverlayWidgetObserver& operator=(
      const ImmersiveModeOverlayWidgetObserver&) = delete;
  ~ImmersiveModeOverlayWidgetObserver() override;

  // views::WidgetObserver:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

 private:
  raw_ptr<ImmersiveModeControllerMac> controller_;
};

class ImmersiveModeControllerMac : public ImmersiveModeController,
                                   public views::FocusChangeListener,
                                   public views::ViewObserver,
                                   public views::WidgetObserver,
                                   public views::FocusTraversable,
                                   public views::ImmersiveModeRevealClient {
 public:
  class RevealedLock : public ImmersiveRevealedLock {
   public:
    explicit RevealedLock(base::WeakPtr<ImmersiveModeControllerMac> controller);

    RevealedLock(const RevealedLock&) = delete;
    RevealedLock& operator=(const RevealedLock&) = delete;

    ~RevealedLock() override;

   private:
    base::WeakPtr<ImmersiveModeControllerMac> controller_;
  };

  // If `separate_tab_strip` is true, the tab strip is split out into its own
  // widget separate from the overlay view so that it can live in the title bar.
  explicit ImmersiveModeControllerMac(bool separate_tab_strip);

  ImmersiveModeControllerMac(const ImmersiveModeControllerMac&) = delete;
  ImmersiveModeControllerMac& operator=(const ImmersiveModeControllerMac&) =
      delete;

  ~ImmersiveModeControllerMac() override;

  // ImmersiveModeController overrides:
  void Init(BrowserView* browser_view) override;
  void SetEnabled(bool enabled) override;
  bool IsEnabled() const override;
  bool ShouldHideTopViews() const override;
  bool IsRevealed() const override;
  int GetTopContainerVerticalOffset(
      const gfx::Size& top_container_size) const override;
  std::unique_ptr<ImmersiveRevealedLock> GetRevealedLock(
      AnimateReveal animate_reveal) override;
  void OnFindBarVisibleBoundsChanged(
      const gfx::Rect& new_visible_bounds_in_screen) override;
  bool ShouldStayImmersiveAfterExitingFullscreen() override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  int GetMinimumContentOffset() const override;
  int GetExtraInfobarOffset() const override;
  void OnContentFullscreenChanged(bool is_content_fullscreen) override;

  // Set the widget id of the tab hosting widget. Set before calling SetEnabled.
  void SetTabNativeWidgetID(uint64_t widget_id);

  // views::FocusChangeListener implementation.
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::ViewObserver implementation
  void OnViewBoundsChanged(views::View* observed_view) override;

  // views::WidgetObserver implementation
  void OnWidgetDestroying(views::Widget* widget) override;

  // views::Traversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

  // views::ImmersiveModeRevealClient:
  void OnImmersiveModeToolbarRevealChanged(bool is_revealed) override;
  void OnImmersiveModeMenuBarRevealChanged(double reveal_amount) override;
  void OnAutohidingMenuBarHeightChanged(int menu_bar_height) override;

  BrowserView* browser_view() { return browser_view_; }

 private:
  friend class RevealedLock;

  void LockDestroyed();

  // Move children from `from_widget` to `to_widget`. Certain child widgets will
  // be held back from the move, see `ShouldMoveChild` for details.
  void MoveChildren(views::Widget* from_widget, views::Widget* to_widget);

  // Returns true if the child should be moved.
  bool ShouldMoveChild(views::Widget* child);

  gfx::Insets GetTabStripRegionViewInsets();

  raw_ptr<BrowserView> browser_view_ = nullptr;  // weak
  std::unique_ptr<ImmersiveRevealedLock> focus_lock_;
  bool enabled_ = false;
  base::ScopedObservation<views::View, views::ViewObserver>
      top_container_observation_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      browser_frame_observation_{this};
  ImmersiveModeOverlayWidgetObserver overlay_widget_observer_{this};
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      overlay_widget_observation_{&overlay_widget_observer_};
  remote_cocoa::mojom::NativeWidgetNSWindow* GetNSWindowMojo();
  std::unique_ptr<views::FocusSearch> focus_search_;

  // Used to hold the widget id for the tab hosting widget. This will be passed
  // to the remote_cocoa immersive mode controller where the tab strip will be
  // placed in the titlebar.
  uint64_t tab_native_widget_id_ = 0;

  // Whether the tab strip should be a separate widget.
  bool separate_tab_strip_ = false;
  // Height of the tab widget, used when resizing. Only non-zero if
  // `separate_tab_strip_` is true.
  int tab_widget_height_ = 0;
  // Total height of the overlay (including the separate tab strip if relevant).
  int overlay_height_ = 0;
  // Whether the find bar is currently visible.
  bool find_bar_visible_ = false;
  // Whether the toolbar is currently visible.
  bool is_revealed_ = false;
  // The proportion of the menubar/topchrome that has been revealed as a result
  // of the user mousing to the top of the screen.
  double reveal_amount_ = 0;
  // The height of the menubar, if the menubar should be accounted for when
  // compensating for reveal animations, otherwise 0. Situations where it is
  // not accounted for include screens with notches (where there is always
  // space reserved for it) and the "Always Show Menu Bar" system setting.
  int menu_bar_height_ = 0;

  std::unique_ptr<views::BoundsAnimator> tab_bounds_animator_ = nullptr;

  base::WeakPtrFactory<ImmersiveModeControllerMac> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_MODE_CONTROLLER_MAC_H_
