// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/base/models/image_model.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace lens {
namespace {

void LogShown(bool shown, std::string invocation_source) {
  base::UmaHistogramBoolean("Lens.Overlay.PermissionBubble.Shown", shown);
  const auto histogram_name =
      "Lens.Overlay.PermissionBubble.ByInvocationSource." + invocation_source +
      ".Shown";
  base::UmaHistogramBoolean(histogram_name, shown);
}

void LogUserAction(LensPermissionBubbleController::UserAction user_action,
                   std::string invocation_source) {
  base::UmaHistogramEnumeration("Lens.Overlay.PermissionBubble.UserAction",
                                user_action);
  const auto histogram_name =
      "Lens.Overlay.PermissionBubble.ByInvocationSource." + invocation_source +
      ".UserAction";
  base::UmaHistogramEnumeration(histogram_name, user_action);
}

}  // namespace

LensPermissionBubbleController::LensPermissionBubbleController(
    BrowserWindowInterface* browser_window_interface,
    PrefService* pref_service,
    std::string invocation_source)
    : invocation_source_(invocation_source),
      browser_window_interface_(browser_window_interface),
      pref_service_(pref_service) {}

LensPermissionBubbleController::~LensPermissionBubbleController() {
  if (HasOpenDialogWidget()) {
    dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

void LensPermissionBubbleController::RequestPermission(
    content::WebContents* web_contents,
    RequestPermissionCallback callback) {
  // Return early if there is already an open dialog widget. Show the dialog if
  // it is not currently visible for some reason.
  if (HasOpenDialogWidget()) {
    if (!dialog_widget_->IsVisible()) {
      dialog_widget_->Show();
    }
    LogShown(false, invocation_source_);
    return;
  }
  LogShown(true, invocation_source_);

  // Observe pref changes. Reset the pref observer in case this method called
  // several times in succession.
  pref_observer_.Reset();
  pref_observer_.Init(pref_service_);
  pref_observer_.Add(
      prefs::kLensSharingPageScreenshotEnabled,
      base::BindRepeating(
          &LensPermissionBubbleController::OnPermissionPreferenceUpdated,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));

  // Show a tab-modal dialog and keep a reference to its widget.
  dialog_widget_ = constrained_window::ShowWebModal(
      CreateLensPermissionDialogModel(), web_contents);
}

std::unique_ptr<ui::DialogModel>
LensPermissionBubbleController::CreateLensPermissionDialogModel() {
  ui::DialogModelLabel::TextReplacement link = ui::DialogModelLabel::CreateLink(
      IDS_LENS_PERMISSION_BUBBLE_DIALOG_LEARN_MORE_LINK,
      base::BindRepeating(
          &LensPermissionBubbleController::OnHelpCenterLinkClicked,
          weak_ptr_factory_.GetWeakPtr()));

  return ui::DialogModel::Builder()
      .SetInternalName(kLensPermissionDialogName)
      .SetTitle(
          l10n_util::GetStringUTF16(IDS_LENS_PERMISSION_BUBBLE_DIALOG_TITLE))
      .SetBannerImage(ui::ImageModel::FromImageSkia(
          *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
              IDR_LENS_PERMISSION_MODAL_IMAGE)))
      .AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
          IDS_LENS_PERMISSION_BUBBLE_DIALOG_DESCRIPTION, link))
      .AddOkButton(
          base::BindOnce(
              &LensPermissionBubbleController::OnPermissionDialogAccept,
              weak_ptr_factory_.GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_LENS_PERMISSION_BUBBLE_DIALOG_CONTINUE_BUTTON))
              .SetId(kLensPermissionDialogOkButtonElementId)
              .SetStyle(ui::ButtonStyle::kProminent))
      .AddCancelButton(
          base::BindOnce(
              &LensPermissionBubbleController::OnPermissionDialogCancel,
              weak_ptr_factory_.GetWeakPtr()),
          ui::DialogModel::Button::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_LENS_PERMISSION_BUBBLE_DIALOG_CANCEL_BUTTON))
              .SetId(kLensPermissionDialogCancelButtonElementId)
              .SetStyle(ui::ButtonStyle::kTonal))
      .SetCloseActionCallback(base::BindOnce(
          &LensPermissionBubbleController::OnPermissionDialogClose,
          weak_ptr_factory_.GetWeakPtr()))
      .Build();
}

bool LensPermissionBubbleController::HasOpenDialogWidget() {
  return dialog_widget_ && !dialog_widget_->IsClosed();
}

void LensPermissionBubbleController::OnHelpCenterLinkClicked(
    const ui::Event& event) {
  LogUserAction(UserAction::kLinkOpened, invocation_source_);
  browser_window_interface_->OpenURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromEventFlags(event.flags(),
                                    WindowOpenDisposition::NEW_BACKGROUND_TAB));
}

void LensPermissionBubbleController::OnPermissionDialogAccept() {
  LogUserAction(UserAction::kAcceptButtonPressed, invocation_source_);
  pref_service_->SetBoolean(prefs::kLensSharingPageScreenshotEnabled, true);
  dialog_widget_ = nullptr;
}

void LensPermissionBubbleController::OnPermissionDialogCancel() {
  LogUserAction(UserAction::kCancelButtonPressed, invocation_source_);
  dialog_widget_ = nullptr;
}

void LensPermissionBubbleController::OnPermissionDialogClose() {
  if (dialog_widget_->closed_reason() ==
      views::Widget::ClosedReason::kEscKeyPressed) {
    LogUserAction(UserAction::kEscKeyPressed, invocation_source_);
  }
  dialog_widget_ = nullptr;
}

void LensPermissionBubbleController::OnPermissionPreferenceUpdated(
    RequestPermissionCallback callback) {
  if (CanSharePageScreenshotWithLensOverlay(pref_service_)) {
    if (HasOpenDialogWidget()) {
      dialog_widget_->CloseWithReason(
          views::Widget::ClosedReason::kAcceptButtonClicked);
    }
    pref_observer_.Reset();
    callback.Run();
  }
}

}  // namespace lens
