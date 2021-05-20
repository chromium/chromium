// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_util.h"

#include <memory>

#include "ash/public/cpp/window_properties.h"
#include "base/callback_forward.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "components/arc/compat_mode/resize_confirmation_dialog_view.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

constexpr gfx::Size kPortraitPhoneDp(412, 732);
constexpr gfx::Size kLandscapeTabletDp(1064, 600);

using ResizeCallback = base::OnceCallback<void(views::Widget*)>;

void ResizeToPhone(views::Widget* widget) {
  if (widget->IsMaximized())
    widget->Restore();
  widget->CenterWindow(kPortraitPhoneDp);
}

void ResizeToTablet(views::Widget* widget) {
  if (widget->IsMaximized())
    widget->Restore();
  widget->CenterWindow(kLandscapeTabletDp);
}

void ResizeToDesktop(views::Widget* widget) {
  widget->Maximize();
}

void ProceedResizeWithConfirmationIfNeeded(
    views::Widget* target_widget,
    ArcResizeLockPrefDelegate* pref_delegate,
    ResizeCallback resize_callback) {
  const auto* app_id =
      target_widget->GetNativeWindow()->GetProperty(ash::kAppIDKey);
  if (app_id && !pref_delegate->GetResizeLockNeedsConfirmation(*app_id)) {
    // The user has already agreed not to show the dialog again.
    std::move(resize_callback).Run(target_widget);
    return;
  }

  // Set target app window as parent so that the dialog will be destroyed
  // together when the app window is destroyed (e.g. app crashed).
  ShowResizeConfirmationDialog(
      /*parent=*/target_widget->GetNativeWindow(),
      base::BindOnce(
          [](views::Widget* widget, ArcResizeLockPrefDelegate* delegate,
             ResizeCallback callback, bool accepted, bool do_not_ask_again) {
            if (accepted) {
              const auto* app_id =
                  widget->GetNativeWindow()->GetProperty(ash::kAppIDKey);
              if (do_not_ask_again && app_id)
                delegate->SetResizeLockNeedsConfirmation(*app_id, false);

              std::move(callback).Run(widget);
            }
          },
          base::Unretained(target_widget), base::Unretained(pref_delegate),
          std::move(resize_callback)));
}

}  // namespace

void ResizeToPhoneWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  ProceedResizeWithConfirmationIfNeeded(widget, pref_delegate,
                                        base::BindOnce(&ResizeToPhone));
}

void ResizeToTabletWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  ProceedResizeWithConfirmationIfNeeded(widget, pref_delegate,
                                        base::BindOnce(&ResizeToTablet));
}

void ResizeToDesktopWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  ProceedResizeWithConfirmationIfNeeded(widget, pref_delegate,
                                        base::BindOnce(&ResizeToDesktop));
}

}  // namespace arc
