// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/external_focus_tracker.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget_delegate.h"

class BrowserView;
class FindBarController;
class FindInPageTest;

namespace find_in_page {
class FindNotificationDetails;
}

namespace views {
class Widget;
}
////////////////////////////////////////////////////////////////////////////////
//
// The FindBarHost implements the container widget for the find-in-page
// functionality. It is responsible for showing, hiding, closing, and moving the
// widget if needed, for example if the widget is obscuring the selection
// results. It also receives notifications about the search results and
// communicates that to the view. There is one FindBarHost per BrowserView, and
// its state is updated whenever the selected Tab is changed. The FindBarHost is
// created when the BrowserView is attached to the frame's Widget for the first
// time.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarHost : public FindBar,
                    public FindBarTesting,
                    public views::FocusChangeListener,
                    public ui::AcceleratorTarget,
                    public views::AnimationDelegateViews,
                    public views::WidgetDelegate {
 public:
  explicit FindBarHost(BrowserView* browser_view);

  FindBarHost(const FindBarHost&) = delete;
  FindBarHost& operator=(const FindBarHost&) = delete;

  ~FindBarHost() override;

  // Forwards selected key events to the renderer. This is useful to make sure
  // that arrow keys and PageUp and PageDown result in scrolling, instead of
  // being eaten because the FindBar has focus. Returns true if the keystroke
  // was forwarded, false if not.
  bool MaybeForwardKeyEventToWebpage(const ui::KeyEvent& key_event);

  // Returns true if the find bar view is visible, or false otherwise.
  bool IsVisible() const;

  // TODO(https://crbug.com/40183900): Remove this and migrate the caller to
  // something more specific.
  BrowserView* browser_view() { return browser_view_; }

#if BUILDFLAG(IS_MAC)
  // Get the host widget.
  views::Widget* GetHostWidget() override;
#endif

  // FindBar implementation:
  FindBarController* GetFindBarController() const override;
  void SetFindBarController(FindBarController* find_bar_controller) override;
  void Show(bool animate) override;
  void Hide(bool animate) override;
  void SetFocusAndSelection() override;
  void ClearResults(
      const find_in_page::FindNotificationDetails& results) override;
  void StopAnimation() override;
  void MoveWindowIfNecessary() override;
  void SetFindTextAndSelectedRange(const std::u16string& find_text,
                                   const gfx::Range& selected_range) override;
  std::u16string GetFindText() const override;
  gfx::Range GetSelectedRange() const override;
  void UpdateUIForFindResult(
      const find_in_page::FindNotificationDetails& result,
      const std::u16string& find_text) override;
  void AudibleAlert() override;
  bool IsFindBarVisible() const override;
  void RestoreSavedFocus() override;
  bool HasGlobalFindPasteboard() const override;
  void UpdateFindBarForChangedWebContents() override;
  const FindBarTesting* GetFindBarTesting() const override;

  // Overridden from ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // FindBarTesting implementation:
  bool GetFindBarWindowInfo(gfx::Point* position,
                            bool* fully_visible) const override;
  std::u16string GetFindSelectedText() const override;
  std::u16string GetMatchCountText() const override;
  int GetContentsWidth() const override;
  size_t GetAudibleAlertCount() const override;

  // views::WidgetDelegate:
  std::u16string GetAccessibleWindowTitle() const override;

  FindBarView* GetFindBarViewForTesting();
  static void SetEnableAnimationsForTesting(bool enable_animations);

 private:
  friend class FindInPageTest;
  friend class LegacyFindInPageTest;

  // Allows implementation to tweak widget position.
  void GetWidgetPositionNative(gfx::Rect* avoid_overlapping_rect);

  // If the find bar obscures the search results we need to move the window. To
  // do that we need to know what is selected on the page. We simply calculate
  // where it would be if we place it on the left of the selection and if it
  // doesn't fit on the screen we try the right side. The parameter
  // |selection_rect| is expected to have coordinates relative to the top of
  // the web page area.
  void MoveWindowIfNecessaryWithRect(const gfx::Rect& selection_rect);

  // Saves the focus tracker for potential restoration later during a
  // WebContents change.
  void SaveFocusTracker();

  // Takes the focus tracker from a WebContents and restores it to the
  // FindBarHost. If no focus tracker is set, creates one.
  void RestoreOrCreateFocusTracker();

  // Called when `is_visible_` changes.
  void OnVisibilityChanged();

  // Registers this class as the handler for when Escape is pressed. Once we
  // loose focus we will unregister Escape and (any accelerators the derived
  // classes registers by using overrides of RegisterAccelerators). See also:
  // SetFocusChangeListener().
  void RegisterAccelerators();

  // When we lose focus, we unregister all accelerator handlers. See also:
  // SetFocusChangeListener().
  void UnregisterAccelerators();

  // Returns the rectangle representing where to position the find bar. It uses
  // GetDialogBounds and positions itself within that, either to the left (if an
  // InfoBar is present) or to the right (no InfoBar). If
  // |avoid_overlapping_rect| is specified, the return value will be a rectangle
  // located immediately to the left of |avoid_overlapping_rect|, as long as
  // there is enough room for the dialog to draw within the bounds. If not, the
  // dialog position returned will overlap |avoid_overlapping_rect|.
  // Note: |avoid_overlapping_rect| is expected to use coordinates relative to
  // the top of the page area, (it will be converted to coordinates relative to
  // the top of the browser window, when comparing against the dialog
  // coordinates). The returned value is relative to the browser window.
  gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect);
  // Moves the dialog window to the provided location, moves it to top in the
  // z-order (HWND_TOP, not HWND_TOPMOST) and shows the window (if hidden).
  void SetDialogPosition(const gfx::Rect& new_pos);

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // views::AnimationDelegateViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Our view, which is responsible for drawing the UI.
  raw_ptr<FindBarView, DanglingUntriaged> view_ = nullptr;

  // The animation class to use when opening the FindBar widget.
  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Host is the Widget implementation that is created and maintained by the
  // find bar. It contains the find bar view.
  std::unique_ptr<views::Widget> host_;

  // A pointer back to the owning controller.
  raw_ptr<FindBarController> find_bar_controller_ = nullptr;

  // The number of audible alerts issued.
  size_t audible_alerts_ = 0;

  // A flag to manually manage visibility. GTK/X11 is asynchronous and
  // the state of the widget can be out of sync.
  bool is_visible_ = false;

  // The BrowserView that created us.
  const raw_ptr<BrowserView, DanglingUntriaged> browser_view_;

  // The focus manager we register with to keep track of focus changes.
  raw_ptr<views::FocusManager, DanglingUntriaged> focus_manager_ = nullptr;

  // True if the accelerator target for Esc key is registered.
  bool esc_accel_target_registered_ = false;

  // Tracks and stores the last focused view which is not the FindBarHost's view
  // or any of its children. Used to restore focus once the FindBarHost's view
  // is closed.
  std::unique_ptr<views::ExternalFocusTracker> focus_tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_
