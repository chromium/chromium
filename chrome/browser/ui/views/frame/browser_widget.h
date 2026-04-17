// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WIDGET_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WIDGET_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include <optional>
#endif

class BrowserFrameView;
class BrowserRootView;
enum class BrowserThemeChangeType;
class BrowserView;
class BrowserNativeWidget;
class SystemMenuModelBuilder;

namespace input {
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
}  // namespace views

enum class TabDragKind {
  // No drag is active.
  kNone,

  // One or more (but not all) tabs within a window are being dragged.
  kTab,

  // All of the tabs in a window are being dragged, and the whole window is
  // along for the ride.
  kAllTabs,
};

// This is a virtual interface that allows system specific browser widgets.
class BrowserWidget : public views::Widget,
                      public views::ContextMenuController {
 public:
  explicit BrowserWidget(BrowserView* browser_view);

  BrowserWidget(const BrowserWidget&) = delete;
  BrowserWidget& operator=(const BrowserWidget&) = delete;

  ~BrowserWidget() override;

#if BUILDFLAG(IS_LINUX)
  // Returns whether the frame is in a tiled state.
  bool tiled() const { return tiled_; }
  void set_tiled(bool tiled) { tiled_ = tiled; }
#endif

  // Initialize the frame (creates the underlying native window).
  void InitBrowserWidget();

  // Returns the FrameView of this frame.
  BrowserFrameView* GetFrameView() const;

  // Returns true when the window placement should be saved.
  bool ShouldSaveWindowPlacement() const;

  // Retrieves the window placement (show state and bounds) for restoring.
  void GetWindowPlacement(gfx::Rect* bounds,
                          ui::mojom::WindowShowState* show_state) const;

  // Returns HANDLED if the |event| was handled by the platform implementation
  // before sending it to the renderer. E.g., it may be swallowed by a native
  // menu bar. Returns NOT_HANDLED_IS_SHORTCUT if the event was not handled, but
  // would be handled as a shortcut if the renderer chooses not to handle it.
  // Otherwise returns NOT_HANDLED.
  content::KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const input::NativeWebKeyboardEvent& event);

  // Returns true if the |event| was handled by the platform implementation,
  // if the renderer did not process it.
  bool HandleKeyboardEvent(const input::NativeWebKeyboardEvent& event);

  // ThemeService calls this when a user has changed their theme, indicating
  // that it's time to redraw everything.
  void UserChangedTheme(BrowserThemeChangeType theme_change_type);

  // views::Widget:
  views::internal::RootView* CreateRootView() override;
  std::unique_ptr<views::FrameView> CreateFrameView() override;
  bool GetAccelerator(int command_id,
                      ui::Accelerator* accelerator) const override;
  const ui::ThemeProvider* GetThemeProvider() const override;
  ui::ColorProviderKey::ThemeInitializerSupplier* GetCustomTheme()
      const override;
  void OnNativeWidgetWorkspaceChanged() override;
  void OnNativeWidgetDestroyed() override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(
      views::View* source,
      const gfx::Point& p,
      ui::mojom::MenuSourceType source_type) override;

  // Returns whether MenuRunner is running or not. Useful to check if the system
  // context menu is showing, when menu_runner_ is used.
  bool IsMenuRunnerRunningForTesting() const;

  // Returns the menu model. BrowserWidget owns the returned model.
  // Note that in multi user mode this will upon each call create a new model.
  ui::MenuModel* GetSystemMenuModel();

  BrowserNativeWidget* browser_native_widget() const {
    return browser_native_widget_;
  }

  void SetTabDragKind(TabDragKind tab_drag_kind);
  TabDragKind tab_drag_kind() const { return tab_drag_kind_; }

 protected:
  // views::Widget:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;
  ui::ColorProviderKey GetColorProviderKey() const override;

 private:
  void OnTouchUiChanged();

  // Callback for MenuRunner.
  void OnMenuClosed();

  // Select a native theme that is appropriate for the current context. This is
  // currently only needed for Linux to switch between the regular NativeTheme
  // and the GTK NativeTheme instance.
  void SelectNativeTheme();

  // Regenerate the frame on theme change if necessary. Returns true if
  // regenerated.
  bool RegenerateFrameOnThemeChange(BrowserThemeChangeType theme_change_type);

  // Returns true if the browser instance belongs to an incognito profile.
  bool IsIncognitoBrowser() const;

  raw_ptr<BrowserNativeWidget> browser_native_widget_;

  // A weak reference to the root view associated with the window. We save a
  // copy as a BrowserRootView to avoid evil casting later, when we need to call
  // functions that only exist on BrowserRootView (versus RootView).
  raw_ptr<BrowserRootView> root_view_;

  // A pointer to our FrameView as a BrowserFrameView.
  raw_ptr<BrowserFrameView> browser_frame_view_;

  // The BrowserView is our ClientView. This is a pointer to it.
  raw_ptr<BrowserView> browser_view_;

  std::unique_ptr<SystemMenuModelBuilder> menu_model_builder_;

  // Used to show the system menu. Only used if
  // BrowserNativeWidget::UsesNativeSystemMenu() returns false.
  std::unique_ptr<views::MenuRunner> menu_runner_;

  base::CallbackListSubscription subscription_ =
      ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&BrowserWidget::OnTouchUiChanged,
                              base::Unretained(this)));

  // Indicates the drag state for this window. The value can be kWindowDrag
  // if the accociated browser is the dragged browser or kTabDrag
  // if this is the source browser that the drag window originates from. During
  // tab dragging process, the dragged browser or the source browser's bounds
  // may change, the fast resize strategy will be used to resize its web
  // contents for smoother dragging.
  TabDragKind tab_drag_kind_ = TabDragKind::kNone;

#if BUILDFLAG(IS_LINUX)
  bool tiled_ = false;
#endif
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_WIDGET_H_
