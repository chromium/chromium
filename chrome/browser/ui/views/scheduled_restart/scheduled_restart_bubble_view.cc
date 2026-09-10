// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/scheduled_restart/scheduled_restart_bubble_view.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/lifetime/scheduled_restart_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/widget/widget.h"

namespace scheduled_restart {

DEFINE_ELEMENT_IDENTIFIER_VALUE(kScheduledRestartDialogId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kRestartNowButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kRestartWhenIdleButtonId);

namespace {

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
  ScheduledRestartDialogDelegate() = default;
  ~ScheduledRestartDialogDelegate() override {
    if (!action_taken_) {
      action_taken_ = true;
      RecordDialogChoice(
          ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kDismissed);
    }
  }

  void OnRestartNowClicked() {
    action_taken_ = true;
    RecordDialogChoice(
        ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kRestartNow);
    base::RecordAction(base::UserMetricsAction("ScheduledRestart_RestartNow"));
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
    if (browser) {
      if (auto* browser_view =
              BrowserView::GetBrowserViewForBrowser(browser.get())) {
        browser_view->contents_web_view()->RequestFocus();
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

  void OnClose() {
    if (!action_taken_) {
      action_taken_ = true;
      RecordDialogChoice(
          ScheduledRestartBubbleView::ScheduledRestartDialogChoice::kDismissed);
      base::RecordAction(base::UserMetricsAction("ScheduledRestart_Close"));
    }
  }

 private:
  bool action_taken_ = false;
};

}  // namespace

// static
std::unique_ptr<views::Widget> ScheduledRestartBubbleView::ShowBubble(
    BrowserWindowInterface* browser,
    views::Widget::ClosedCallback on_close) {
  CHECK(browser);

  auto* toolbar_button_provider = ToolbarButtonProvider::From(browser);
  if (!toolbar_button_provider) {
    return nullptr;
  }

  auto* control = toolbar_button_provider->GetAppMenuControl();
  if (!control) {
    return nullptr;
  }

  auto dialog_delegate = std::make_unique<ScheduledRestartDialogDelegate>();
  auto* delegate_ptr = dialog_delegate.get();

  ui::DialogModel::Builder builder(std::move(dialog_delegate));
  // Passing 0 explicitly selects the general non-deadline "=0 {A Chrome update
  // is available}" branch, already branded and localized in Chrome and
  // Chromium strings.
  builder
      .SetTitle(
          l10n_util::GetPluralStringFUTF16(IDS_RELAUNCH_RECOMMENDED_TITLE, 0))
      .OverrideShowCloseButton(true)
      .SetElementIdentifier(kScheduledRestartDialogId)
      .AddParagraph(ui::DialogModelLabel(
          l10n_util::GetStringUTF16(IDS_RELAUNCH_RECOMMENDED_BODY_SCHEDULE)))
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
                         base::Unretained(delegate_ptr)),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_RELAUNCH_RECOMMENDED_RESTART_NOW))
              .SetStyle(ui::ButtonStyle::kProminent)
              .SetId(kRestartNowButtonId))
      .SetCloseActionCallback(
          base::BindOnce(&ScheduledRestartDialogDelegate::OnClose,
                         base::Unretained(delegate_ptr)));

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      builder.Build(), control->GetAnchor(), views::BubbleBorder::TOP_RIGHT);

  const auto* layout_provider = ChromeLayoutProvider::Get();
  bubble->set_fixed_width(layout_provider->GetDistanceMetric(
      views::DISTANCE_LARGE_MODAL_DIALOG_PREFERRED_WIDTH));

  gfx::Insets margins = bubble->margins();
  margins.set_bottom(margins.bottom() +
                     layout_provider->GetDistanceMetric(
                         views::DISTANCE_UNRELATED_CONTROL_VERTICAL));
  bubble->set_margins(margins);

  std::unique_ptr<views::Widget> widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble).release(),
                                                std::move(on_close));
  if (widget) {
    widget->Show();
    base::RecordAction(base::UserMetricsAction("ScheduledRestart_BubbleShown"));
  }
  return widget;
}

// static
void ScheduledRestartBubbleView::set_relaunch_callback_for_testing(
    base::RepeatingClosure callback) {
  GetRelaunchCallbackForTesting() = std::move(callback);  // IN-TEST
}

}  // namespace scheduled_restart
