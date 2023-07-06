// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_
#define CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/views/dropdown_bar_host.h"
#include "chrome/browser/ui/views/find_bar_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/textfield/textfield.h"

class BrowserView;
class FindBarController;
class FindInPageTest;

namespace find_in_page {
class FindNotificationDetails;
}

////////////////////////////////////////////////////////////////////////////////
//
// The FindBarHost implements the container widget for the
// find-in-page functionality. It uses the implementation from
// find_bar_host_aura.cc to draw its content and is responsible for showing,
// hiding, closing, and moving the widget if needed, for example if the widget
// is obscuring the selection results. It also receives notifications about the
// search results and communicates that to the view.
//
// There is one FindBarHost per BrowserView, and its state is updated
// whenever the selected Tab is changed. The FindBarHost is created when
// the BrowserView is attached to the frame's Widget for the first time.
//
////////////////////////////////////////////////////////////////////////////////
class FindBarHost : public DropdownBarHost,
                    public FindBar,
                    public FindBarTesting {
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

  // Overridden from ui::AcceleratorTarget in DropdownBarHost class:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // FindBarTesting implementation:
  bool GetFindBarWindowInfo(gfx::Point* position,
                            bool* fully_visible) const override;
  std::u16string GetFindSelectedText() const override;
  std::u16string GetMatchCountText() const override;
  int GetContentsWidth() const override;
  size_t GetAudibleAlertCount() const override;

  // Overridden from DropdownBarHost:
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
  gfx::Rect GetDialogPosition(gfx::Rect avoid_overlapping_rect) override;
  // Moves the dialog window to the provided location, moves it to top in the
  // z-order (HWND_TOP, not HWND_TOPMOST) and shows the window (if hidden).
  // It then calls UpdateWindowEdges to make sure we don't overwrite the Chrome
  // window border.
  void SetDialogPosition(const gfx::Rect& new_pos) override;

  // Retrieves the boundaries that the find bar widget has to work with
  // within the Chrome frame window. The resulting rectangle will be a
  // rectangle that overlaps the bottom of the Chrome toolbar by one
  // pixel (so we can create the illusion that the dropdown widget is
  // part of the toolbar) and covers the page area, except that we
  // deflate the rect width by subtracting (from both sides) the width
  // of the toolbar and some extra pixels to account for the width of
  // the Chrome window borders. |bounds| is relative to the browser
  // window. If the function fails to determine the browser
  // window/client area rectangle or the rectangle for the page area
  // then |bounds| will be an empty rectangle.
  void GetWidgetBounds(gfx::Rect* bounds) override;

  // Additional accelerator handling (on top of what DropDownBarHost does).
  void RegisterAccelerators() override;
  void UnregisterAccelerators() override;

#if BUILDFLAG(IS_MAC)
  // Get the host widget.
  views::Widget* GetHostWidget() override;
#endif

 protected:
  // Overridden from DropdownBarHost:
  void OnVisibilityChanged() override;

  // views::WidgetDelegate:
  std::u16string GetAccessibleWindowTitle() const override;

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

  // Returns the FindBarView.
  FindBarView* find_bar_view() { return static_cast<FindBarView*>(view()); }
  const FindBarView* find_bar_view() const {
    return static_cast<const FindBarView*>(view());
  }

  // Saves the focus tracker for potential restoration later during a
  // WebContents change.
  void SaveFocusTracker();

  // Takes the focus tracker from a WebContents and restores it to the
  // DropdownBarHost.
  void RestoreFocusTracker();

  // A pointer back to the owning controller.
  raw_ptr<FindBarController> find_bar_controller_ = nullptr;

  // The number of audible alerts issued.
  size_t audible_alerts_ = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FIND_BAR_HOST_H_
