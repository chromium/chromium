// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_WINDOW_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/share/share_attempt.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

class BrowserWindowInterface;
class SharingDialog;

namespace content {
class WebContents;
}  // namespace content

// Manages the sharing functionality for a browser window.
class SharingWindowController {
 public:
  DECLARE_USER_DATA(SharingWindowController);

  explicit SharingWindowController(BrowserWindowInterface* browser);
  SharingWindowController(const SharingWindowController&) = delete;
  SharingWindowController& operator=(const SharingWindowController&) = delete;
  ~SharingWindowController();

  static SharingWindowController* From(BrowserWindowInterface* browser);

  // Shows the dialog for a sharing feature.
  SharingDialog* ShowSharingDialog(content::WebContents* contents,
                                   SharingDialogData data);

 private:
  friend class ui::ScopedUnownedUserData<SharingWindowController>;

  const raw_ref<BrowserWindowInterface> browser_;
  ui::ScopedUnownedUserData<SharingWindowController> scoped_unowned_user_data_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_SHARING_WINDOW_CONTROLLER_H_
