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
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace views {
class Widget;
class DialogDelegate;
}  // namespace views

namespace tabs {

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
  void ShowDialogAndBlockTabInteraction(views::Widget* dialog);
  // Combines the above two functions into a single invocation. This is the most
  // commonly used version. Only use the other APIs if the caller must do
  // something unique to the Widget before showing it.
  std::unique_ptr<views::Widget> CreateShowDialogAndBlockTabInteraction(
      views::DialogDelegate* delegate);

  void CloseDialog();

 private:
  // Overridden from content::WebContentObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  void OnWidgetDestroyed(views::Widget* widget);

  void TabDidEnterForeground(TabInterface* tab_interface);
  void TabWillEnterBackground(TabInterface* tab_interface);
  void TabWillDetach(TabInterface* tab_interface,
                     TabInterface::DetachReason reason);

  raw_ptr<TabInterface> tab_interface_ = nullptr;
  base::CallbackListSubscription tab_did_enter_foreground_subscription_;
  base::CallbackListSubscription tab_will_enter_background_subscription_;
  base::CallbackListSubscription tab_will_detach_subscription_;

  // Active dialog.
  base::WeakPtr<views::Widget> widget_;

  std::optional<content::WebContents::ScopedIgnoreInputEvents>
      scoped_ignore_input_events_;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_
