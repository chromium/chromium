// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_view.h"

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/chrome_typography.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_hats.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace scheduled_restart {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kScheduledRestartDialogId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kScheduledRestartBodyId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kRestartNowButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kRestartWhenIdleButtonId);

namespace {

// Set while a bubble is destroyed because a new bubble is replacing it. The
// user did not dismiss it, so the destruction must not be recorded as a
// dismissal.
bool g_is_replacing_bubble = false;

base::RepeatingClosure& GetRelaunchCallbackForTesting() {
  static base::NoDestructor<base::RepeatingClosure> callback;
  return *callback;
}

void RecordDialogChoice(
    ScheduledRestartBubbleView::ScheduledRestartDialogChoice choice) {
  base::UmaHistogramEnumeration("Session.ScheduledRestart.DialogChoice",
                                choice);
}

class ScheduledRestartDialogDelegate : public ui::DialogModelDelegate {
 public:
  explicit ScheduledRestartDialogDelegate(bool show_active)
      : show_active_(show_active) {}
  ~ScheduledRestartDialogDelegate() override {
    if (!action_taken_ && !g_is_replacing_bubble) {
      action_taken_ = true;
      RecordDialogChoice(
          ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kDismissed);
    }
  }

  void OnRestartNowClicked(base::WeakPtr<BrowserWindowInterface> browser) {
    action_taken_ = true;
    RecordDialogChoice(
        ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kRestartNow);
    base::RecordAction(base::UserMetricsAction("ScheduledRestart_RestartNow"));
    LaunchSurveyForChoice(
        ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kRestartNow,
        browser.get());
    if (GetRelaunchCallbackForTesting()) {
      GetRelaunchCallbackForTesting().Run();  // IN-TEST
      return;
    }
    chrome::AttemptRelaunch();
  }

  void OnRestartWhenIdleClicked(base::WeakPtr<BrowserWindowInterface> browser,
                                const ui::Event& event) {
    action_taken_ = true;
    RecordDialogChoice(ScheduledRestartBubbleView::
                           ScheduledRestartDialogChoice::kScheduledOnIdle);
    base::RecordAction(base::UserMetricsAction("ScheduledRestart_Scheduled"));
    auto* srm =
        g_browser_process && g_browser_process->GetFeatures()
            ? g_browser_process->GetFeatures()->scheduled_restart_manager()
            : nullptr;
    if (srm) {
      srm->ScheduleRestartOnIdle();
    }
    LaunchSurveyForChoice(ScheduledRestartBubbleView::
                              ScheduledRestartDialogChoice::kScheduledOnIdle,
                          browser.get());
    if (browser) {
      if (auto* browser_view =
              BrowserView::GetBrowserViewForBrowser(browser.get())) {
        browser_view->GetContentsView()->RequestFocus();
      }
      if (auto* toast_controller = ToastController::From(browser.get())) {
        toast_controller->MaybeShowToast(
            ToastParams(ToastId::kScheduledRestartOnIdle));
      }
    }
    if (dialog_model() && dialog_model()->host()) {
      dialog_model()->host()->Close();
    }
  }

  void OnClose(base::WeakPtr<BrowserWindowInterface> browser) {
    if (!action_taken_ && !g_is_replacing_bubble) {
      action_taken_ = true;
      RecordDialogChoice(
          ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kDismissed);
      base::RecordAction(base::UserMetricsAction("ScheduledRestart_Close"));
      LaunchSurveyForChoice(
          ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kDismissed,
          browser.get());
    }
  }

  base::WeakPtr<ScheduledRestartDialogDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void LaunchSurveyForChoice(
      ScheduledRestartBubbleView::ScheduledRestartDialogChoice choice,
      BrowserWindowInterface* browser) {
    scheduled_restart::MaybeLaunchHatsSurvey(
        choice, browser, show_active_, base::TimeTicks::Now() - show_time_);
  }

  const bool show_active_;
  const base::TimeTicks show_time_ = base::TimeTicks::Now();
  bool action_taken_ = false;
  base::WeakPtrFactory<ScheduledRestartDialogDelegate> weak_ptr_factory_{this};
};

// Returns true if the widget closure was initiated by an explicit user action
// (clicking the close button, pressing Escape, or clicking outside the bubble
// for an active bubble). Programmatic closes (e.g. `Widget::Close()`, which
// passes `kUnspecified`) are not considered user dismissals.
bool IsUserDismissal(views::Widget::ClosedReason reason) {
  switch (reason) {
    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kLostFocus:
      return true;
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kCancelButtonClicked:
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      return false;
  }
  NOTREACHED();
}

// Chained close callback. `delegate` is passed via `WeakPtr` because the
// synchronous close callback is owned by the Widget and can outlive the
// DialogModel host/delegate if destruction ordering differs.
void RunCloseActionThenClosedCallback(
    base::WeakPtr<ScheduledRestartDialogDelegate> delegate,
    base::WeakPtr<BrowserWindowInterface> browser,
    views::Widget::ClosedCallback callback,
    views::Widget::ClosedReason reason) {
  if (delegate && IsUserDismissal(reason)) {
    // If an action was already taken (e.g. Accept or Restart when idle),
    // `ScheduledRestartDialogDelegate::OnClose` is a no-op via its internal
    // `action_taken_` guard.
    delegate->OnClose(browser);
  }
  std::move(callback).Run(reason);
}

}  // namespace

// static
std::unique_ptr<views::Widget> ScheduledRestartBubbleView::ShowBubble(
    BrowserWindowInterface* browser,
    views::Widget::ClosedCallback on_close,
    bool show_active) {
  CHECK(browser);

  auto* toolbar_button_provider = ToolbarButtonProvider::From(browser);
  if (!toolbar_button_provider) {
    return nullptr;
  }

  auto* control = toolbar_button_provider->GetAppMenuControl();
  if (!control) {
    return nullptr;
  }

  auto dialog_delegate =
      std::make_unique<ScheduledRestartDialogDelegate>(show_active);
  auto* delegate_ptr = dialog_delegate.get();

  ui::DialogModel::Builder builder(std::move(dialog_delegate));
  builder
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_SCHEDULED_TITLE))
      .OverrideShowCloseButton(true)
      .SetElementIdentifier(kScheduledRestartDialogId)
      .AddParagraph(
          ui::DialogModelLabel::CreateWithReplacements(
              IDS_RELAUNCH_RECOMMENDED_BODY_SCHEDULE,
              {ui::DialogModelLabel::CreateEmphasizedText(
                   l10n_util::GetStringUTF16(
                       IDS_RELAUNCH_RECOMMENDED_BODY_SCHEDULE_IMPORTANT_UPDATE)),
               ui::DialogModelLabel::CreateEmphasizedText(FormatIdleDuration(
                   ScheduledRestartManager::GetIdleThreshold()))}),
          /*header=*/std::u16string(), kScheduledRestartBodyId)
      .AddExtraButton(
          base::BindRepeating(
              &ScheduledRestartDialogDelegate::OnRestartWhenIdleClicked,
              base::Unretained(delegate_ptr), browser->GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_RELAUNCH_RECOMMENDED_RESTART_WHEN_IDLE))
              .SetStyle(ui::ButtonStyle::kTonal)
              .SetId(kRestartWhenIdleButtonId))
      .AddOkButton(
          base::BindOnce(&ScheduledRestartDialogDelegate::OnRestartNowClicked,
                         base::Unretained(delegate_ptr), browser->GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_RELAUNCH_RECOMMENDED_RESTART_NOW))
              .SetStyle(ui::ButtonStyle::kProminent)
              .SetId(kRestartNowButtonId))
      // In production, `on_close` is always provided by the controller and
      // intercepted synchronously via `Widget::MakeCloseSynchronous()`.
      // `SetCloseActionCallback` remains as a fallback for callers that pass a
      // null `on_close`, and (together with the close button) keeps Escape
      // mapped to a close rather than a cancel.
      .SetCloseActionCallback(base::BindOnce(
          &ScheduledRestartDialogDelegate::OnClose,
          base::Unretained(delegate_ptr), browser->GetWeakPtr()));
  if (!show_active) {
    builder.SetIsAlertDialog();
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      builder.Build(), control->GetAnchor(), views::BubbleBorder::TOP_RIGHT);

  const auto* layout_provider = ChromeLayoutProvider::Get();
  bubble->set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  // When a non-null `on_close` callback is passed to `CreateBubble()`, Views
  // registers it via `Widget::MakeCloseSynchronous()`. That causes
  // `Widget::CloseWithReason()` (invoked on every widget close, including
  // X-button click, Escape, focus loss, and programmatic `Close()`) to run
  // `on_close` and return early before reaching
  // `DialogDelegate::WindowWillClose()`, where `SetCloseActionCallback` is
  // normally dispatched. Chain `OnClose()` ahead of `on_close` for user
  // dismissals so dismissal metrics and the HaTS survey still run when the
  // widget closes synchronously.
  if (on_close) {
    on_close = base::BindOnce(&RunCloseActionThenClosedCallback,
                              delegate_ptr->GetWeakPtr(), browser->GetWeakPtr(),
                              std::move(on_close));
  }

  std::unique_ptr<views::Widget> widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble).release(),
                                                std::move(on_close));
  if (widget) {
    if (show_active) {
      widget->Show();
    } else {
      // Show as inactive so the bubble does not steal focus (e.g. from the
      // Omnibox when opening a New Tab Page), and add an accessible description
      // explaining how to focus the bubble.
      widget->GetRootView()->GetViewAccessibility().SetDescription(
          l10n_util::GetStringUTF16(IDS_SHOW_BUBBLE_INACTIVE_DESCRIPTION));
      widget->ShowInactive();
    }
    base::RecordAction(base::UserMetricsAction("ScheduledRestart_BubbleShown"));
  }
  return widget;
}

// static
void ScheduledRestartBubbleView::DestroyReplacedBubble(
    std::unique_ptr<views::Widget> widget) {
  // Widget destruction synchronously destroys the dialog delegate, so the flag
  // only needs to be held for the duration of the reset.
  base::AutoReset<bool> replacing(&g_is_replacing_bubble, true);
  widget.reset();
}

// static
std::u16string ScheduledRestartBubbleView::FormatIdleDuration(
    base::TimeDelta idle_threshold) {
  std::u16string duration =
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, idle_threshold);
  base::ReplaceSubstringsAfterOffset(&duration, 0, u" ", u"\u00A0");
  return duration;
}

// static
void ScheduledRestartBubbleView::set_relaunch_callback_for_testing(
    base::RepeatingClosure callback) {
  GetRelaunchCallbackForTesting() = std::move(callback);  // IN-TEST
}

}  // namespace scheduled_restart
