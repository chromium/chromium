// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_
#define CHROME_BROWSER_UI_TABS_PUBLIC_TAB_DIALOG_MANAGER_H_

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
}  // namespace views

namespace tabs {

// Class provides a mechanism to show a tab-scoped dialog.
class TabDialogManager : public content::WebContentsObserver {
 public:
  explicit TabDialogManager(TabInterface* tab_interface);
  TabDialogManager(const TabDialogManager&) = delete;
  TabDialogManager& operator=(const TabDialogManager&) = delete;
  ~TabDialogManager() override;

  void ShowDialogAndBlockTabInteraction(views::Widget* dialog);

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
