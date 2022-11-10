// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/base/ui_base_types.h"
#endif

class BrowserNonClientFrameView;
class BrowserRootView;
enum class BrowserThemeChangeType;
class BrowserView;
class NativeBrowserFrame;
class NonClientFrameView;
class SystemMenuModelBuilder;

namespace content {
struct NativeWebKeyboardEvent;
}

namespace gfx {
class Rect;
}

namespace ui {
class MenuModel;
}

namespace views {
class MenuRunner;
class View;
}

enum class TabDragKind {
  // No drag is active.
  kNone,

  // One or more (but not all) tabs within a window are being dragged.
  kTab,

  // All of the tabs in a window are being dragged, and the whole window is
  // along for the ride.
  kAllTabs,
};

// This is a virtual interface that allows system specific browser frames.
class BrowserFrame : public views::Widget, public views::ContextMenuController {
 public:
  explicit BrowserFrame(BrowserView* browser_view);

  BrowserFrame(const BrowserFrame&) = delete;
  BrowserFrame& operator=(const BrowserFrame&) = delete;

  ~BrowserFrame() override;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Returns which edges of the frame are tiled.
  const ui::WindowTiledEdges& tiled_edges() const { return tiled_edges_; }
  void set_tiled_edges(ui::WindowTiledEdges tiled_edges) {
    tiled_edges_ = tiled_edges;
  }
#endif

  // Initialize the frame (creates the underlying native window).
  void InitBrowserFrame();

  // Determine the distance of the left edge of the minimize button from the
  // left edge of the window. Used in our Non-Client View's Layout.
  int GetMinimizeButtonOffset() const;

  // Retrieves the bounds in non-client view coordinates for the
  // TabStripRegionView that contains the TabStrip view.
  gfx::Rect GetBoundsForTabStripRegion(
      const gfx::Size& tabstrip_minimum_size) const;

  // Returns the inset of the topmost view in the client view from the top of
  // the non-client view. The topmost view depends on the window type. The
  // topmost view is the tab strip for tabbed browser windows, the toolbar for
  // popups, the web contents for app windows and varies for fullscreen windows.
  int GetTopInset() const;

  // Returns the amount that the theme background should be inset.
  int GetThemeBackgroundXInset() const;

  // Tells the frame to update the throbber.
  void UpdateThrobber(bool running);

  // Returns the NonClientFrameView of this frame.
  BrowserNonClientFrameView* GetFrameView() const;

  // Returns |true| if we should use the custom frame.
  bool UseCustomFrame() const;

  // Returns true when the window placement should be saved.
  bool ShouldSaveWindowPlacement() const;

  // Returns true when a frame header should be drawn.
  virtual bool ShouldDrawFrameHeader() const;

  // Retrieves the window placement (show state and bounds) for restoring.
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::WindowShowState* show_state) const;

  // Returns HANDLED if the |event| was handled by the platform implementation
  // before sending it to the renderer. E.g., it may be swallowed by a native
  // menu bar. Returns NOT_HANDLED_IS_SHORTCUT if the event was not handled, but
  // would be handled as a shortcut if the renderer chooses not to handle it.
  // Otherwise returns NOT_HANDLED.
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event);

  // Returns true if the |event| was handled by the platform implementation,
  // if the renderer did not process it.
  bool HandleKeyboardEvent(const content::NativeWebKeyboardEvent& event);

  // Called when BrowserView creates all it's child views.
  void OnBrowserViewInitViewsComplete();

  // ThemeService calls this when a user has changed their theme, indicating
  // that it's time to redraw everything.
  void UserChangedTheme(BrowserThemeChangeType theme_change_type);

  // views::Widget:
  views::internal::RootView* CreateRootView() override;
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView()
      override;
  bool GetAccelerator(int command_id,
                      ui::Accelerator* accelerator) const override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  ui::ColorProviderManager::ThemeInitializerSupplier* GetCustomTheme()
      const override;
  void OnNativeWidgetWorkspaceChanged() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& p,
                                  ui::MenuSourceType source_type) override;

  // Returns the menu model. BrowserFrame owns the returned model.
  // Note that in multi user mode this will upon each call create a new model.
  ui::MenuModel* GetSystemMenuModel();

  NativeBrowserFrame* native_browser_frame() const {
    return native_browser_frame_;
  }

  void SetTabDragKind(TabDragKind tab_drag_kind);
  TabDragKind tab_drag_kind() const { return tab_drag_kind_; }

 protected:
  // views::Widget:
  ui::ColorProviderManager::Key GetColorProviderKey() const override;

 private:
  void OnTouchUiChanged();

  // Callback for MenuRunner.
  void OnMenuClosed();

  // Select a native theme that is appropriate for the current context.
  void SelectNativeTheme();

  // Regenerate the frame on theme change if necessary. Returns true if
  // regenerated.
  bool RegenerateFrameOnThemeChange(BrowserThemeChangeType theme_change_type);

  // Returns whether the browser should always use the dark theme no matter user
  // makes any selection.
  bool ShouldUseDarkTheme() const;

  raw_ptr<NativeBrowserFrame> native_browser_frame_;

  // A weak reference to the root view associated with the window. We save a
  // copy as a BrowserRootView to avoid evil casting later, when we need to call
  // functions that only exist on BrowserRootView (versus RootView).
  raw_ptr<BrowserRootView> root_view_;

  // A pointer to our NonClientFrameView as a BrowserNonClientFrameView.
  raw_ptr<BrowserNonClientFrameView> browser_frame_view_;

  // The BrowserView is our ClientView. This is a pointer to it.
  raw_ptr<BrowserView> browser_view_;

  std::unique_ptr<SystemMenuModelBuilder> menu_model_builder_;

  // Used to show the system menu. Only used if
  // NativeBrowserFrame::UsesNativeSystemMenu() returns false.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserFrame::OnTouchUiChanged,
                              base::Unretained(this)));

  // Indicates the drag state for this window. The value can be kWindowDrag
  // if the accociated browser is the dragged browser or kTabDrag
  // if this is the source browser that the drag window originates from. During
  // tab dragging process, the dragged browser or the source browser's bounds
  // may change, the fast resize strategy will be used to resize its web
  // contents for smoother dragging.
  TabDragKind tab_drag_kind_ = TabDragKind::kNone;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS)
  ui::WindowTiledEdges tiled_edges_;
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Store the number of virtual desks that currently exist. Used to determine
  // whether the system menu should be reset. If the value is -1, then either
  // the ash::DesksHelper does not exist or haven't retrieved the system menu
  // model yet.
  int num_desks_ = -1;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_FRAME_H_
