// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/critical_notification_bubble_view.h"

#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/locale_settings.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/style/typography.h"
#include "ui/views/widget/widget.h"

using base::UserMetricsAction;

////////////////////////////////////////////////////////////////////////////////
// CriticalNotificationBubbleView

CriticalNotificationBubbleView::CriticalNotificationBubbleView(
    views::View* anchor_view)
    : BubbleDialogDelegateView(anchor_view, views::BubbleBorder::TOP_RIGHT) {
  DialogDelegate::set_button_label(
      ui::DIALOG_BUTTON_OK,
      l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_RESTART));
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   l10n_util::GetStringUTF16(IDS_CANCEL));
  set_close_on_deactivate(false);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::CRITICAL_NOTIFICATION);
}

CriticalNotificationBubbleView::~CriticalNotificationBubbleView() {
}

base::TimeDelta CriticalNotificationBubbleView::GetRemainingTime() const {
  // How long to give the user until auto-restart if no action is taken.
  constexpr auto kCountdownDuration = base::TimeDelta::FromSeconds(30);
  const base::TimeDelta time_lapsed = base::TimeTicks::Now() - bubble_created_;
  return kCountdownDuration - time_lapsed;
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

base::string16 CriticalNotificationBubbleView::GetWindowTitle() const {
  const auto remaining_time = GetRemainingTime();
  return remaining_time > base::TimeDelta()
             ? l10n_util::GetPluralStringFUTF16(IDS_CRITICAL_NOTIFICATION_TITLE,
                                                remaining_time.InSeconds())
             : l10n_util::GetStringUTF16(
                   IDS_CRITICAL_NOTIFICATION_TITLE_ALTERNATE);
}

void CriticalNotificationBubbleView::WindowClosing() {
  refresh_timer_.Stop();
}

bool CriticalNotificationBubbleView::Cancel() {
  UpgradeDetector::GetInstance()->acknowledge_critical_update();
  base::RecordAction(UserMetricsAction("CriticalNotification_Ignore"));
  // If the counter reaches 0, we set a restart flag that must be cleared if
  // the user selects, for example, "Stay on this page" during an
  // onbeforeunload handler.
  PrefService* prefs = g_browser_process->local_state();
  if (prefs->HasPrefPath(prefs::kRestartLastSessionOnShutdown))
    prefs->ClearPref(prefs::kRestartLastSessionOnShutdown);
  return true;
}

bool CriticalNotificationBubbleView::Accept() {
  UpgradeDetector::GetInstance()->acknowledge_critical_update();
  base::RecordAction(UserMetricsAction("CriticalNotification_Restart"));
  chrome::AttemptRestart();
  return true;
}

void CriticalNotificationBubbleView::Init() {
  bubble_created_ = base::TimeTicks::Now();

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto message = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_CRITICAL_NOTIFICATION_TEXT),
      views::style::CONTEXT_MESSAGE_BOX_BODY_TEXT,
      views::style::STYLE_SECONDARY);
  message->SetMultiLine(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SizeToFit(
      ChromeLayoutProvider::Get()->GetDistanceMetric(
          ChromeDistanceMetric::DISTANCE_BUBBLE_PREFERRED_WIDTH) -
      margins().width());
  AddChildView(std::move(message));

  refresh_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                       &CriticalNotificationBubbleView::OnCountdown);

  base::RecordAction(UserMetricsAction("CriticalNotificationShown"));
}

void CriticalNotificationBubbleView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kAlertDialog;
}

void CriticalNotificationBubbleView::ViewHierarchyChanged(
    const views::ViewHierarchyChangedDetails& details) {
  if (details.is_add && details.child == this)
    NotifyAccessibilityEvent(ax::mojom::Event::kAlert, true);
}
