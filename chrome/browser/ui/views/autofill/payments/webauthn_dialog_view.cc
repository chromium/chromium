// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/webauthn_dialog_view.h"

#include "chrome/browser/ui/autofill/payments/webauthn_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_model.h"
#include "chrome/browser/ui/autofill/payments/webauthn_dialog_state.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

WebauthnDialogView::WebauthnDialogView(WebauthnDialogController* controller,
                                       WebauthnDialogState dialog_state)
    : controller_(controller) {
  SetShowTitle(false);
  SetLayoutManager(std::make_unique<views::FillLayout>());
  std::unique_ptr<WebauthnDialogModel> model =
      std::make_unique<WebauthnDialogModel>(dialog_state);
  model_ = model.get();
  model_->AddObserver(this);
  sheet_view_ =
      AddChildView(CreateSheetViewForAutofillWebAuthn(std::move(model)));
  sheet_view_->ReInitChildViews();

  SetModalType(ui::mojom::ModalType::kChild);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetButtonLabel(ui::mojom::DialogButton::kOk, model_->GetAcceptButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 model_->GetCancelButtonLabel());
  SetButtons(model_->IsAcceptButtonVisible()
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kCancel));
}

WebauthnDialogView::~WebauthnDialogView() {
  model_->RemoveObserver(this);
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

// static
WebauthnDialog* WebauthnDialog::CreateAndShow(
    WebauthnDialogController* controller,
    WebauthnDialogState dialog_state) {
  WebauthnDialogView* dialog = new WebauthnDialogView(controller, dialog_state);
  constrained_window::ShowWebModalDialogViews(dialog,
                                              controller->GetWebContents());
  return dialog;
}

WebauthnDialogModel* WebauthnDialogView::GetDialogModel() const {
  return model_;
}

void WebauthnDialogView::OnDialogStateChanged() {
  switch (model_->dialog_state()) {
    case WebauthnDialogState::kInactive:
      Hide();
      break;
    case WebauthnDialogState::kOfferPending:
    case WebauthnDialogState::kOfferError:
    case WebauthnDialogState::kVerifyPending:
      RefreshContent();
      break;
    case WebauthnDialogState::kUnknown:
    case WebauthnDialogState::kOffer:
      NOTREACHED();
  }
}

bool WebauthnDialogView::Accept() {
  DCHECK_EQ(model_->dialog_state(), WebauthnDialogState::kOffer);
  controller_->OnOkButtonClicked();
  return false;
}

bool WebauthnDialogView::Cancel() {
  if (model_->dialog_state() == WebauthnDialogState::kOffer ||
      model_->dialog_state() == WebauthnDialogState::kOfferPending ||
      model_->dialog_state() == WebauthnDialogState::kVerifyPending) {
    controller_->OnCancelButtonClicked();
  }

  return true;
}

bool WebauthnDialogView::IsDialogButtonEnabled(
    ui::mojom::DialogButton button) const {
  return button == ui::mojom::DialogButton::kOk
             ? model_->IsAcceptButtonEnabled()
             : true;
}

std::u16string WebauthnDialogView::GetWindowTitle() const {
  return model_->GetStepTitle();
}

void WebauthnDialogView::Hide() {
  // Reset controller reference if the controller has been destroyed before the
  // view being destroyed. This happens if browser window is closed when the
  // dialog is visible.
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

void WebauthnDialogView::RefreshContent() {
  sheet_view_->ReInitChildViews();
  sheet_view_->InvalidateLayout();
  SetButtonLabel(ui::mojom::DialogButton::kOk, model_->GetAcceptButtonLabel());
  SetButtonLabel(ui::mojom::DialogButton::kCancel,
                 model_->GetCancelButtonLabel());
  DCHECK(model_->IsCancelButtonVisible());
  SetButtons(model_->IsAcceptButtonVisible()
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kCancel));

  DialogModelChanged();
  DeprecatedLayoutImmediately();

  // Update the dialog's size.
  if (GetWidget() && controller_->GetWebContents()) {
    constrained_window::UpdateWebContentsModalDialogPosition(
        GetWidget(), web_modal::WebContentsModalDialogManager::FromWebContents(
                         controller_->GetWebContents())
                         ->delegate()
                         ->GetWebContentsModalDialogHost());
  }
}

BEGIN_METADATA(WebauthnDialogView)
END_METADATA

}  // namespace autofill
