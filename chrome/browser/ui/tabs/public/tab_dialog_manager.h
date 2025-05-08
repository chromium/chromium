// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace views {
class Widget;
class WidgetObserver;
class DialogDelegate;
}  // namespace views

namespace tabs {

class TabDialogWidgetObserver;
class BrowserWindowWidgetObserver;

// Class provides a mechanism to show a tab-scoped dialog.
class TabDialogManager : public content::WebContentsObserver {
 public:
  explicit TabDialogManager(TabInterface* tab_interface);
  TabDialogManager(const TabDialogManager&) = delete;
  TabDialogManager& operator=(const TabDialogManager&) = delete;
  ~TabDialogManager() override;

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
  void ShowDialogAndBlockTabInteraction(views::Widget* dialog,
                                        bool close_on_navigation = true);
  // Combines the above two functions into a single invocation. This is the most
  // commonly used version. Only use the other APIs if the caller must do
  // something unique to the Widget before showing it.
  std::unique_ptr<views::Widget> CreateShowDialogAndBlockTabInteraction(
      views::DialogDelegate* delegate,
      bool close_on_navigation = true);

  void CloseDialog();

  // Resets all state associated with `widget_`.
  // Called in two different circumstances:
  //  * From an internal WidgetObserver when the Widget is in the process of
  //    being destroyed by external code.
  //  * From CloseDialog(), right before calling Close() on the widget.
  void WidgetDestroyed(views::Widget* widget);

 private:
  // Overridden from content::WebContentObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void TabDidEnterForeground(TabInterface* tab_interface);
  void TabWillEnterBackground(TabInterface* tab_interface);
  void TabWillDetach(TabInterface* tab_interface,
                     TabInterface::DetachReason reason);

  raw_ptr<TabInterface> tab_interface_ = nullptr;
  base::CallbackListSubscription tab_did_enter_foreground_subscription_;
  base::CallbackListSubscription tab_will_enter_background_subscription_;
  base::CallbackListSubscription tab_will_detach_subscription_;

  // Active dialog and associated state. These members should be set and cleared
  // simultaneously.
  raw_ptr<views::Widget> widget_;
  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
  std::unique_ptr<TabDialogWidgetObserver> tab_dialog_widget_observer_;
  std::unique_ptr<BrowserWindowWidgetObserver> browser_window_widget_observer_;
  std::unique_ptr<ScopedTabModalUI> showing_modal_ui_;

  bool close_on_navigation_ = true;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_
