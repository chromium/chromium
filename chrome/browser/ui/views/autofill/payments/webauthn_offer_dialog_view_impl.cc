// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/webauthn_offer_dialog_view_impl.h"

#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/webauthn_offer_dialog_model.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/webauthn/authenticator_request_sheet_view.h"
#include "chrome/browser/ui/views/webauthn/sheet_view_factory.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

WebauthnOfferDialogViewImpl::WebauthnOfferDialogViewImpl(
    WebauthnOfferDialogController* controller)
    : controller_(controller) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  std::unique_ptr<WebauthnOfferDialogModel> model =
      std::make_unique<WebauthnOfferDialogModel>();
  model_ = model.get();
  model_->AddObserver(this);
  sheet_view_ =
      AddChildView(CreateSheetViewForAutofillWebAuthn(std::move(model)));
  sheet_view_->ReInitChildViews();

  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   model_->GetAcceptButtonLabel());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   model_->GetCancelButtonLabel());
}

WebauthnOfferDialogViewImpl::~WebauthnOfferDialogViewImpl() {
  model_->RemoveObserver(this);
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

// static
WebauthnOfferDialogView* WebauthnOfferDialogView::CreateAndShow(
    WebauthnOfferDialogController* controller) {
  WebauthnOfferDialogViewImpl* dialog =
      new WebauthnOfferDialogViewImpl(controller);
  constrained_window::ShowWebModalDialogViews(dialog,
                                              controller->GetWebContents());
  return dialog;
}

WebauthnOfferDialogModel* WebauthnOfferDialogViewImpl::GetDialogModel() const {
  return model_;
}

void WebauthnOfferDialogViewImpl::OnDialogStateChanged() {
  switch (model_->dialog_state()) {
    case WebauthnOfferDialogModel::DialogState::kInactive:
      Hide();
      break;
    case WebauthnOfferDialogModel::DialogState::kPending:
    case WebauthnOfferDialogModel::DialogState::kError:
      RefreshContent();
      break;
    case WebauthnOfferDialogModel::DialogState::kUnknown:
    case WebauthnOfferDialogModel::DialogState::kOffer:
      NOTREACHED();
  }
}

gfx::Size WebauthnOfferDialogViewImpl::CalculatePreferredSize() const {
  const int width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH);
  return gfx::Size(width, GetHeightForWidth(width));
}

bool WebauthnOfferDialogViewImpl::Accept() {
  DCHECK_EQ(model_->dialog_state(),
            WebauthnOfferDialogModel::DialogState::kOffer);
  controller_->OnOkButtonClicked();
  return false;
}

bool WebauthnOfferDialogViewImpl::Cancel() {
  if (model_->dialog_state() == WebauthnOfferDialogModel::DialogState::kOffer ||
      model_->dialog_state() ==
          WebauthnOfferDialogModel::DialogState::kPending) {
    controller_->OnCancelButtonClicked();
  }

  return true;
}

bool WebauthnOfferDialogViewImpl::Close() {
  return true;
}

int WebauthnOfferDialogViewImpl::GetDialogButtons() const {
  // Cancel button is always visible but OK button depends on dialog state.
  DCHECK(model_->IsCancelButtonVisible());
  return model_->IsAcceptButtonVisible()
             ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
             : ui::DIALOG_BUTTON_CANCEL;
}

bool WebauthnOfferDialogViewImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? model_->IsAcceptButtonEnabled()
                                        : true;
}

ui::ModalType WebauthnOfferDialogViewImpl::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

base::string16 WebauthnOfferDialogViewImpl::GetWindowTitle() const {
  return model_->GetStepTitle();
}

bool WebauthnOfferDialogViewImpl::ShouldShowWindowTitle() const {
  return false;
}

bool WebauthnOfferDialogViewImpl::ShouldShowCloseButton() const {
  return false;
}

void WebauthnOfferDialogViewImpl::Hide() {
  // Reset controller reference if the controller has been destroyed before the
  // view being destroyed. This happens if browser window is closed when the
  // dialog is visible.
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

void WebauthnOfferDialogViewImpl::RefreshContent() {
  sheet_view_->ReInitChildViews();
  sheet_view_->InvalidateLayout();
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_OK,
                                   model_->GetAcceptButtonLabel());
  DialogDelegate::set_button_label(ui::DIALOG_BUTTON_CANCEL,
                                   model_->GetCancelButtonLabel());
  DialogModelChanged();
  Layout();

  // Update the dialog's size.
  if (GetWidget() && controller_->GetWebContents()) {
    constrained_window::UpdateWebContentsModalDialogPosition(
        GetWidget(), web_modal::WebContentsModalDialogManager::FromWebContents(
                         controller_->GetWebContents())
                         ->delegate()
                         ->GetWebContentsModalDialogHost());
  }
}

}  // namespace autofill
