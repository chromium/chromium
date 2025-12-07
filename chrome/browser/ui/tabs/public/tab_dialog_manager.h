// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class LinearAnimation;
}  // namespace gfx

namespace views {
class Widget;
class WidgetObserver;
class DialogDelegate;
}  // namespace views

namespace tabs {

class TabDialogWidgetObserver;

// Class provides a mechanism to show a tab-scoped dialog on desktop platforms.
// Please file a bug if you encounter any issues.
class TabDialogManager : public content::WebContentsObserver,
                         public gfx::AnimationDelegate {
 public:
  using ShouldShowCallback = base::RepeatingCallback<void(bool&)>;
  using GetDialogBounds = base::RepeatingCallback<gfx::Rect()>;
  explicit TabDialogManager(TabInterface* tab_interface);
  TabDialogManager(const TabDialogManager&) = delete;
  TabDialogManager& operator=(const TabDialogManager&) = delete;
  ~TabDialogManager() override;

  // Parameters that are used to configure the behavior of the tab dialog.
  struct Params {
    Params();
    ~Params();
    // If the tab's main frame performs a different-site navigation, close the
    // dialog.
    bool close_on_navigate = true;

    // If the tab is detached, close the dialog.
    bool close_on_detach = true;

    // Disable input on the underlying WebContents.
    bool disable_input = true;

    // If true, the dialog will animate its bounds changes. This is not
    // compatible with `views::Widget::InitParams::autosize`. The client is
    // responsible for calling `UpdateModalDialogBounds()` when the dialog size
    // changes to trigger the animation.
    bool animated = false;

    // Ensure that TabInterface::CanShowModalUI() reflects whether another modal
    // dialog can be shown while another is currently being shown. When this
    // flag is false, a subsequent dialog will hide/dismiss the existing dialog.
    bool block_new_modal = true;

    // Assign a callback here if the client intends to handle all sizing and
    // positioning responsibilities. Useful for when the dialog is a bubble
    // or similar and needs different positioning logic.
    GetDialogBounds get_dialog_bounds;

    // By default, TabDialogManager will show the widget if the tab is visible,
    // and the browser window is not minimized. This callback can be set to add
    // an additional condition that will be checked to determine widget
    // visibility.
    ShouldShowCallback should_show_callback;

    // If true, the dialog will be shown without activating the window,
    // preventing focus-stealing from another window. This is intended for
    // passive UI like toasts and overlays.
    bool should_show_inactive = false;
  };

  // Create a dialog widget from the given DialogDelegate suitable for showing
  // as scoped to a tab.
  std::unique_ptr<views::Widget> CreateTabScopedDialog(
      views::DialogDelegate* delegate);
  // Shows a widget scoped to the associated `tab_interface_`. Only one
  // tab-scoped dialog widget may be shown at a time. Interrogate
  // TabInterface::CanShowModalUI() to determine whether it is safe to call this
  // function. The dialog is centered above the hosting view as obtained via
  // TabInterface::GetBrowserWindowInterface() and will be shown/hidden along
  // with the tab. Currently, the returned widget's ownership model is still
  // dependent on ownership within the DialogDelegate. If the call-site
  // ownership hasn't been migrated to CLIENT_OWNS_WIDGET or
  // WIDGET_OWNS_NATIVE_WIDGET, do not hold on to the Widget as a unique_ptr.
  // TODO(kylixrd):
  //   (1) Call-sites expect to own the Widget using CLIENT_OWNS_WIDGET and be
  //       updated accordingly.
  void ShowDialog(views::Widget* dialog, std::unique_ptr<Params> params);
  // Combines the above two functions into a single invocation. This is the most
  // commonly used version. Only use the other APIs if the caller must do
  // something special with the widget.
  std::unique_ptr<views::Widget> CreateAndShowDialog(
      views::DialogDelegate* delegate,
      std::unique_ptr<Params> params);

  void CloseDialog();

  // Activates the dialog if applicable. Returns true if the dialog was
  // activated. This returns false in a few cases:
  //  * If there is no dialog.
  //  * If the associated tab is not active.
  //  * If the browser window is minimized.
  bool MaybeActivateDialog();

  // Resets all state associated with `widget_`.
  // Called in two different circumstances:
  //  * From an internal WidgetObserver when the Widget is in the process of
  //    being destroyed by external code.
  //  * From CloseDialog(), right before calling Close() on the widget.
  void WidgetDestroyed(views::Widget* widget);

  // Returns the widget associated with the browser window. This widget is used
  // as the parent for tab-scoped widgets.
  views::Widget* GetHostWidget() const;

  // Updates the bounds of the modal dialog. If `Params::animated` is true, this
  // will animate the bounds change. Clients with non-autosized dialogs should
  // call this when the dialog's preferred size changes.
  void UpdateModalDialogBounds();

  // Updates the modal dialog host the dialog is associated with from the
  // browser window.
  void UpdateModalDialogHost();

  // Trigger the dialog manager to re-evaluate the dialog's visibility.
  // Optionally pass in a `requested_visibility` which is the state the client
  // thinks the dialog should be in, assuming the tab is visible and the window
  // is not minimized. This will also make sure the `should_show_callback` is
  // properly invoked and update the widget's visibility accordingly. Clients
  // should call this when their internal state has changed which may affect the
  // currently showing dialog's visibility. Function returns the new visibility
  // state of the dialog.
  bool UpdateDialogVisibility(
      std::optional<bool> requested_visibility = std::nullopt);

  // Returns whether this given dialog is already under management or not.
  bool IsDialogManaged(views::Widget* widget);

  // Overridden from gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  class BrowserWindowWidgetObserver;
  class WebContentsModalDialogHostObserver;
  friend class BrowserWindowWidgetObserver;
  friend class WebContentsModalDialogHostObserver;
  //  Overridden from content::WebContentObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameWasResized(bool width_changed) override;

  void TabDidEnterForeground(TabInterface* tab_interface);
  void TabWillEnterBackground(TabInterface* tab_interface);
  void TabWillDetach(TabInterface* tab_interface,
                     TabInterface::DetachReason reason);

  bool GetDialogWidgetVisibility();

  raw_ptr<TabInterface> tab_interface_ = nullptr;

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Active dialog and associated state. These members should be set and cleared
  // simultaneously.
  raw_ptr<views::Widget> widget_;
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
  std::unique_ptr<TabDialogWidgetObserver> tab_dialog_widget_observer_;
  std::unique_ptr<BrowserWindowWidgetObserver> browser_window_widget_observer_;
  std::unique_ptr<WebContentsModalDialogHostObserver>
      web_contents_modal_dialog_host_observer_;
  std::unique_ptr<ScopedTabModalUI> showing_modal_ui_;
  std::unique_ptr<Params> params_;

  // For animating the dialog bounds.
  gfx::Rect animation_start_bounds_;
  gfx::Rect animation_target_bounds_;
  std::unique_ptr<gfx::LinearAnimation> bounds_animation_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_
