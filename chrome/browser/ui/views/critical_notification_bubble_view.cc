// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/critical_notification_bubble_view.h"

#include "base/i18n/time_formatting.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;

CriticalNotificationBubbleView::TimeFormatter g_time_formatter =
    &base::TimeDurationFormatWithSeconds;

CriticalNotificationBubbleView::ScopedSetTimeFormatterForTesting::
    ScopedSetTimeFormatterForTesting(TimeFormatter time_formatter)
    : resetter_(&g_time_formatter, time_formatter) {}

CriticalNotificationBubbleView::ScopedSetTimeFormatterForTesting::
    ~ScopedSetTimeFormatterForTesting() = default;

CriticalNotificationBubbleView::CriticalNotificationBubbleView(
    views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT) {
  SetButtonLabel(ui::mojom::DialogButton::kOk,
                 l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_RESTART));
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 l10n_util::GetStringUTF16(IDS_CANCEL));
  SetAcceptCallback(
      base::BindOnce(&CriticalNotificationBubbleView::OnDialogAccepted,
                     base::Unretained(this)));
  SetCancelCallback(
      base::BindOnce(&CriticalNotificationBubbleView::OnDialogCancelled,
                     base::Unretained(this)));
  set_close_on_deactivate(false);

  GetViewAccessibility().SetRole(ax::mojom::Role::kAlertDialog);
}

CriticalNotificationBubbleView::~CriticalNotificationBubbleView() {
}

base::TimeDelta CriticalNotificationBubbleView::GetRemainingTime() const {
  // How long to give the user until auto-restart if no action is taken.
  const base::TimeDelta time_lapsed = base::TimeTicks::Now() - bubble_created_;
  return base::Seconds(30) - time_lapsed;
}

void CriticalNotificationBubbleView::OnCountdown() {
  UpgradeDetector* upgrade_detector = UpgradeDetector::GetInstance();
  if (upgrade_detector->critical_update_acknowledged()) {
    // The user has already interacted with the bubble and chosen a path.
    GetWidget()->Close();
    return;
  }

  if (GetRemainingTime() <= base::TimeDelta()) {
    // Time's up!
    upgrade_detector->acknowledge_critical_update();

    base::RecordAction(UserMetricsAction("CriticalNotification_AutoRestart"));
    refresh_timer_.Stop();
    chrome::AttemptRestart();
  }

  // Update the counter. It may seem counter-intuitive to update the message
  // after we attempt restart, but remember that shutdown may be aborted by
  // an onbeforeunload handler, leaving the bubble up when the browser should
  // have restarted (giving the user another chance).
  GetBubbleFrameView()->UpdateWindowTitle();
}

std::u16string CriticalNotificationBubbleView::GetWindowTitle() const {
  const auto remaining_time = GetRemainingTime();
  std::u16string formatted_time;
  return remaining_time.is_positive() &&
                 (*g_time_formatter)(remaining_time, base::DURATION_WIDTH_WIDE,
                                     &formatted_time)
             ? l10n_util::GetStringFUTF16(IDS_CRITICAL_NOTIFICATION_TITLE,
                                          formatted_time)
             : l10n_util::GetStringUTF16(
                   IDS_CRITICAL_NOTIFICATION_TITLE_ALTERNATE);
}

void CriticalNotificationBubbleView::WindowClosing() {
  refresh_timer_.Stop();
}

void CriticalNotificationBubbleView::OnDialogCancelled() {
  UpgradeDetector::GetInstance()->acknowledge_critical_update();
  base::RecordAction(UserMetricsAction("CriticalNotification_Ignore"));
  // If the counter reaches 0, we set a restart flag that must be cleared if
  // the user selects, for example, "Stay on this page" during an
  // onbeforeunload handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown))
    prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
}

void CriticalNotificationBubbleView::OnDialogAccepted() {
  UpgradeDetector::GetInstance()->acknowledge_critical_update();
  base::RecordAction(UserMetricsAction("CriticalNotification_Restart"));
  chrome::AttemptRestart();
}

void CriticalNotificationBubbleView::Init() {
  bubble_created_ = base::TimeTicks::Now();

  SetUseDefaultFillLayout(true);

  auto message = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_TEXT),
      views::style::CONTEXT_DIALOG_BODY_TEXT, views::style::STYLE_SECONDARY);
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SizeToFit(ChromeLayoutProvider::Get()->GetDistanceMetric(
                         views::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
                     margins().width());
  AddChildView(std::move(message));

  refresh_timer_.Start(FROM_HERE, base::Seconds(1), this,
                       &CriticalNotificationBubbleView::OnCountdown);

  base::RecordAction(UserMetricsAction("CriticalNotificationShown"));
}

void CriticalNotificationBubbleView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}

BEGIN_METADATA(CriticalNotificationBubbleView)
END_METADATA
