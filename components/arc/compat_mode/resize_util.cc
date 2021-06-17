// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/resize_util.h"

#include <memory>

#include "ash/public/cpp/toast_data.h"
#include "ash/public/cpp/toast_manager.h"
#include "ash/public/cpp/window_properties.h"
#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "base/stl_util.h"
#include "components/arc/compat_mode/arc_resize_lock_pref_delegate.h"
#include "components/arc/compat_mode/resize_confirmation_dialog_view.h"
#include "components/strings/grit/components_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
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

absl::optional<std::string> GetAppId(views::Widget* widget) {
  const auto* app_id = widget->GetNativeWindow()->GetProperty(ash::kAppIDKey);
  return base::OptionalFromPtr(app_id);
}

void TurnOnResizeLock(views::Widget* widget,
                      ArcResizeLockPrefDelegate* pref_delegate) {
  const auto app_id = GetAppId(widget);
  if (app_id && pref_delegate->GetResizeLockState(*app_id) !=
                    mojom::ArcResizeLockState::ON) {
    pref_delegate->SetResizeLockState(*app_id, mojom::ArcResizeLockState::ON);
  }
}

void TurnOffResizeLock(views::Widget* target_widget,
                       ArcResizeLockPrefDelegate* pref_delegate) {
  const auto app_id = GetAppId(target_widget);
  if (!app_id || pref_delegate->GetResizeLockState(*app_id) ==
                     mojom::ArcResizeLockState::OFF) {
    return;
  }

  pref_delegate->SetResizeLockState(*app_id, mojom::ArcResizeLockState::OFF);

  auto* const toast_manager = ash::ToastManager::Get();
  // |toast_manager| can be null in some unittests.
  if (!toast_manager)
    return;

  constexpr char kTurnOffResizeLockToastId[] =
      "arc.compat_mode.turn_off_resize_lock";
  constexpr int kToastDurationMs = 3500;
  toast_manager->Cancel(kTurnOffResizeLockToastId);
  ash::ToastData toast(
      kTurnOffResizeLockToastId,
      l10n_util::GetStringUTF16(IDS_ARC_COMPAT_MODE_DISABLE_RESIZE_LOCK_TOAST),
      kToastDurationMs,
      /*dismiss_text=*/absl::nullopt,
      /*visible_on_lock_screen=*/false);
  toast_manager->Show(toast);
}

void TurnOffResizeLockWithConfirmationIfNeeded(
    views::Widget* target_widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  const auto app_id = GetAppId(target_widget);
  if (app_id && !pref_delegate->GetResizeLockNeedsConfirmation(*app_id)) {
    // The user has already agreed not to show the dialog again.
    TurnOffResizeLock(target_widget, pref_delegate);
    return;
  }

  // Set target app window as parent so that the dialog will be destroyed
  // together when the app window is destroyed (e.g. app crashed).
  ShowResizeConfirmationDialog(
      /*parent=*/target_widget->GetNativeWindow(),
      base::BindOnce(
          [](views::Widget* widget, ArcResizeLockPrefDelegate* delegate,
             bool accepted, bool do_not_ask_again) {
            if (accepted) {
              const auto app_id = GetAppId(widget);
              if (do_not_ask_again && app_id)
                delegate->SetResizeLockNeedsConfirmation(*app_id, false);

              TurnOffResizeLock(widget, delegate);
            }
          },
          base::Unretained(target_widget), base::Unretained(pref_delegate)));
}

}  // namespace

void ResizeLockToPhone(views::Widget* widget,
                       ArcResizeLockPrefDelegate* pref_delegate) {
  ResizeToPhone(widget);
  TurnOnResizeLock(widget, pref_delegate);
}

void ResizeLockToTablet(views::Widget* widget,
                        ArcResizeLockPrefDelegate* pref_delegate) {
  ResizeToTablet(widget);
  TurnOnResizeLock(widget, pref_delegate);
}

void EnableResizingWithConfirmationIfNeeded(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  TurnOffResizeLockWithConfirmationIfNeeded(widget, pref_delegate);
}

absl::optional<ResizeCompatMode> PredictCurrentMode(
    views::Widget* widget,
    ArcResizeLockPrefDelegate* pref_delegate) {
  const int width = widget->GetWindowBoundsInScreen().width();
  const int height = widget->GetWindowBoundsInScreen().height();
  const auto app_id = GetAppId(widget);
  // We don't use the exact size here to predict tablet or phone size because
  // the window size might be bigger than it due to the ARC app-side minimum
  // size constraints.
  if (app_id && pref_delegate->GetResizeLockState(*app_id) !=
                    mojom::ArcResizeLockState::ON)
    return ResizeCompatMode::kResizable;
  else if (width < height)
    return ResizeCompatMode::kPhone;
  else if (width > height)
    return ResizeCompatMode::kTablet;
  return absl::nullopt;
}

bool ShouldShowSplashScreenDialog(ArcResizeLockPrefDelegate* pref_delegate) {
  int show_count = pref_delegate->GetShowSplashScreenDialogCount();
  if (show_count == 0)
    return false;

  pref_delegate->SetShowSplashScreenDialogCount(--show_count);
  return true;
}

}  // namespace arc
