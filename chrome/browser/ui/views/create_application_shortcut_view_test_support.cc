// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/create_application_shortcut_view_test_support.h"

#include "base/callback_list.h"
#include "base/check.h"
#include "base/run_loop.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/test/dialog_test.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

CreateChromeApplicationShortcutViewWaiter::
    CreateChromeApplicationShortcutViewWaiter()
    : waiter_(views::test::AnyWidgetTestPasskey{},
              "CreateChromeApplicationShortcutView") {}

void CreateChromeApplicationShortcutViewWaiter::WaitForAndAccept() && {
  views::Widget* widget = waiter_.WaitIfNeededAndGet();
  CHECK(widget != nullptr);

  // Wait until the "ok" button is enabled (which in practice corresponds to
  // waiting until `CreateChromeApplicationShortcutView::OnAppInfoLoaded` is
  // called).
  views::View* ok_button =
      widget->widget_delegate()->AsDialogDelegate()->GetOkButton();
  while (!ok_button->GetEnabled()) {
    base::RunLoop run_loop;
    base::CallbackListSubscription subscription =
        ok_button->AddEnabledChangedCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

  views::test::AcceptDialog(widget);
}
