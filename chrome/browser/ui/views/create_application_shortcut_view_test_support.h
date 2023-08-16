// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_TEST_SUPPORT_H_
#define CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_TEST_SUPPORT_H_

#include "ui/views/widget/any_widget_observer.h"

// A specialization of `NamedWidgetShownWaiter` for waiting for + working with
// the "CreateChromeApplicationShortcut" dialog.  The specialization is
// desirable mostly because `NamedWidgetShownWaiter::WaitIfNeededAndGet` is not
// sufficient in this case, because we also need to wait until the dialog
// fetches the app info and enables the "ok" button.
//
// As with `NamedWidgetShownWaiter` it is important that
// `CreateChromeApplicationShortcutViewWaiter` be constructed before any code
// that might show the dialog.
class CreateChromeApplicationShortcutViewWaiter {
 public:
  CreateChromeApplicationShortcutViewWaiter();

  // Waits 1) until the "CreateChromeApplicationShortcut" dialog is shown *and*
  // 2) until the dialog can be accepted and *then* 3) accepts the dialog.
  void WaitForAndAccept() &&;

 private:
  views::NamedWidgetShownWaiter waiter_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_CREATE_APPLICATION_SHORTCUT_VIEW_TEST_SUPPORT_H_
