// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/security_token_restriction/security_token_session_restriction_view.h"

#include <string>

#include "base/i18n/message_formatter.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace {

constexpr base::TimeDelta kCountdownUpdateInterval = base::Milliseconds(1000);
constexpr base::TimeDelta kLastUpdateTime = base::Milliseconds(1000);

std::u16string GetTitle(
    ash::login::SecurityTokenSessionController::Behavior behavior) {
  switch (behavior) {
    case ash::login::SecurityTokenSessionController::Behavior::kLogout:
      return l10n_util::GetStringUTF16(
          IDS_SECURITY_TOKEN_SESSION_LOGOUT_NOTIFICATION_TITLE);
    case ash::login::SecurityTokenSessionController::Behavior::kLock:
      return l10n_util::GetStringFUTF16(
          IDS_SECURITY_TOKEN_SESSION_LOCK_NOTIFICATION_TITLE,
          ui::GetChromeOSDeviceName());
    case ash::login::SecurityTokenSessionController::Behavior::kIgnore:
      // Intentionally falling through to NOTREACHED().
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string GetButtonLabel(
    ash::login::SecurityTokenSessionController::Behavior behavior) {
  switch (behavior) {
    case ash::login::SecurityTokenSessionController::Behavior::kLogout:
      return l10n_util::GetStringUTF16(
          IDS_SECURITY_TOKEN_SESSION_LOGOUT_NOTIFICATION_BUTTON_TITLE);
    case ash::login::SecurityTokenSessionController::Behavior::kLock:
      return l10n_util::GetStringUTF16(
          IDS_SECURITY_TOKEN_SESSION_LOCK_NOTIFICATION_BUTTON_TITLE);
    case ash::login::SecurityTokenSessionController::Behavior::kIgnore:
      // Intentionally falling through to NOTREACHED().
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

std::u16string GetDialogText(
    ash::login::SecurityTokenSessionController::Behavior behavior,
    const std::string& domain,
    base::TimeDelta time_remaining) {
  // The text and the arguments required for it depend on both `behavior` and
  // `time_remaining`.
  switch (behavior) {
    case ash::login::SecurityTokenSessionController::Behavior::kLogout:
      if (time_remaining <= kLastUpdateTime) {
        return base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_SECURITY_TOKEN_SESSION_IMMEDIATE_LOGOUT_NOTIFICATION_BODY),
            domain);
      }
      return base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SECURITY_TOKEN_SESSION_LOGOUT_NOTIFICATION_BODY),
          time_remaining.InSeconds(), domain);
    case ash::login::SecurityTokenSessionController::Behavior::kLock:
      if (time_remaining <= kLastUpdateTime) {
        return base::i18n::MessageFormatter::FormatWithNumberedArgs(
            l10n_util::GetStringUTF16(
                IDS_SECURITY_TOKEN_SESSION_IMMEDIATE_LOCK_NOTIFICATION_BODY),
            ui::GetChromeOSDeviceName(), domain);
      }
      return base::i18n::MessageFormatter::FormatWithNumberedArgs(
          l10n_util::GetStringUTF16(
              IDS_SECURITY_TOKEN_SESSION_LOCK_NOTIFICATION_BODY),
          time_remaining.InSeconds(), ui::GetChromeOSDeviceName(), domain);
    case ash::login::SecurityTokenSessionController::Behavior::kIgnore:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

}  // namespace

SecurityTokenSessionRestrictionView::SecurityTokenSessionRestrictionView(
    base::TimeDelta duration,
    base::OnceClosure accept_callback,
    ash::login::SecurityTokenSessionController::Behavior behavior,
    const std::string& domain)
    : AppDialogView(ui::ImageModel::FromVectorIcon(chromeos::kEnterpriseIcon,
                                                   ui::kColorIcon,
                                                   20)),
      behavior_(behavior),
      clock_(base::DefaultTickClock::GetInstance()),
      domain_(domain),
      end_time_(base::TimeTicks::Now() + duration) {
  SetModalType(ui::mojom::ModalType::kSystem);
  SetButtonLabel(ui::mojom::DialogButton::kOk, GetButtonLabel(behavior));
  InitializeView();
  AddTitle(GetTitle(behavior));

  SetAcceptCallback(std::move(accept_callback));

  AddSubtitle(/*subtitle_text=*/std::u16string());
  UpdateSubtitle();

  update_timer_.Start(FROM_HERE, kCountdownUpdateInterval, this,
                      &SecurityTokenSessionRestrictionView::UpdateSubtitle);
}

SecurityTokenSessionRestrictionView::~SecurityTokenSessionRestrictionView() =
    default;

void SecurityTokenSessionRestrictionView::UpdateSubtitle() {
  const base::TimeDelta time_remaining = end_time_ - clock_->NowTicks();
  SetSubtitleText(GetDialogText(behavior_, domain_, time_remaining));
  if (time_remaining < kLastUpdateTime) {
    update_timer_.Stop();
  }
}

BEGIN_METADATA(SecurityTokenSessionRestrictionView)
END_METADATA
