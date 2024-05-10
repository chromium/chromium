// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/lens/lens_overlay_permission_utils.h"
#include "chrome/browser/ui/managed_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/common/referrer.h"
#include "ui/base/l10n/l10n_util.h"
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

void LogShown(bool shown) {
  base::UmaHistogramBoolean("Lens.Overlay.PermissionBubble.Shown", shown);
}

void LogUserAction(LensPermissionBubbleController::UserAction user_action) {
  base::UmaHistogramEnumeration("Lens.Overlay.PermissionBubble.UserAction",
                                user_action);
}

}  // namespace

// static
std::unique_ptr<LensPermissionBubbleController>
LensPermissionBubbleController::CreateInstance(Browser* browser,
                                               PrefService* pref_service) {
  if (!browser || !pref_service) {
    return nullptr;
  }
  return std::make_unique<LensPermissionBubbleController>(browser,
                                                          pref_service);
}

LensPermissionBubbleController::LensPermissionBubbleController(
    Browser* browser,
    PrefService* pref_service)
    : browser_(browser), pref_service_(pref_service) {}

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
    LogShown(false);
    return;
  }
  LogShown(true);

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
                  IDS_LENS_PERMISSION_BUBBLE_DIALOG_CLOSE_BUTTON))
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
  LogUserAction(UserAction::kLinkOpened);
  browser_->OpenURL(
      content::OpenURLParams(
          GURL("https://support.google.com/"
               "chrome?p=search_from_page#topic=7439538"),
          content::Referrer(),
          ui::DispositionFromEventFlags(
              event.flags(), WindowOpenDisposition::NEW_BACKGROUND_TAB),
          ui::PAGE_TRANSITION_LINK, false),
      /*navigation_handle_callback=*/{});
}

void LensPermissionBubbleController::OnPermissionDialogAccept() {
  LogUserAction(UserAction::kAcceptButtonPressed);
  pref_service_->SetBoolean(prefs::kLensSharingPageScreenshotEnabled, true);
  dialog_widget_ = nullptr;
}

void LensPermissionBubbleController::OnPermissionDialogCancel() {
  LogUserAction(UserAction::kCancelButtonPressed);
  dialog_widget_ = nullptr;
}

void LensPermissionBubbleController::OnPermissionDialogClose() {
  if (dialog_widget_->closed_reason() ==
      views::Widget::ClosedReason::kEscKeyPressed) {
    LogUserAction(UserAction::kEscKeyPressed);
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
