// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SECURITY_DIALOG_TRACKER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SECURITY_DIALOG_TRACKER_H_

#include "base/no_destructor.h"

namespace views {
class Widget;
}

class Browser;

namespace extensions {

// A singleton class for tracking security dialogs across browser windows and
// profiles. This class provides methods to add and remove dialogs that are
// considered security-sensitive, and to check if any such dialogs are visible
// in a given browser. This class is used to block extensions popups from
// opening when security dialogs are present.
class SecurityDialogTracker {
 public:
  static SecurityDialogTracker* GetInstance();

  SecurityDialogTracker(const SecurityDialogTracker&) = delete;
  SecurityDialogTracker& operator=(const SecurityDialogTracker&) = delete;

  // Adds a security dialog to block extension popups from opening while the
  // dialog is visible.
  // Prefer using z-order levels defined in ChromeWidgetSublevel to prevent
  // spoofing. Only add dialogs to this tracker as a last resort when z-order
  // does not work. See https://crbug.com/40058873#comment41.
  void AddSecurityDialog(views::Widget* widget);

  // Removes a security dialog from the tracker.
  // You don't need to manually remove the dialog on its closing since the
  // tracker will handle that for you.
  void RemoveSecurityDialog(views::Widget* widget);

  // Returns true if `browser` has visible security dialogs.
  bool BrowserHasVisibleSecurityDialogs(Browser* browser) const;

 private:
  friend class base::NoDestructor<SecurityDialogTracker>;

  SecurityDialogTracker();
  ~SecurityDialogTracker();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SECURITY_DIALOG_TRACKER_H_
