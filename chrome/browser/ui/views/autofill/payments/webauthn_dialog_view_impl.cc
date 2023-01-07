// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/webauthn_dialog_view_impl.h"

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
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/fill_layout.h"

namespace autofill {

WebauthnDialogViewImpl::WebauthnDialogViewImpl(
    WebauthnDialogController* controller,
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

  SetModalType(ui::MODAL_TYPE_CHILD);
  SetShowCloseButton(false);
  set_fixed_width(views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_MODAL_DIALOG_PREFERRED_WIDTH));

  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->GetAcceptButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->GetCancelButtonLabel());
  SetButtons(model_->IsAcceptButtonVisible()
                 ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
                 : ui::DIALOG_BUTTON_CANCEL);
}

WebauthnDialogViewImpl::~WebauthnDialogViewImpl() {
  model_->RemoveObserver(this);
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
}

// static
WebauthnDialogView* WebauthnDialogView::CreateAndShow(
    WebauthnDialogController* controller,
    WebauthnDialogState dialog_state) {
  WebauthnDialogViewImpl* dialog =
      new WebauthnDialogViewImpl(controller, dialog_state);
  constrained_window::ShowWebModalDialogViews(dialog,
                                              controller->GetWebContents());
  return dialog;
}

WebauthnDialogModel* WebauthnDialogViewImpl::GetDialogModel() const {
  return model_;
}

void WebauthnDialogViewImpl::OnDialogStateChanged() {
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

bool WebauthnDialogViewImpl::Accept() {
  DCHECK_EQ(model_->dialog_state(), WebauthnDialogState::kOffer);
  controller_->OnOkButtonClicked();
  return false;
}

bool WebauthnDialogViewImpl::Cancel() {
  if (model_->dialog_state() == WebauthnDialogState::kOffer ||
      model_->dialog_state() == WebauthnDialogState::kOfferPending ||
      model_->dialog_state() == WebauthnDialogState::kVerifyPending) {
    controller_->OnCancelButtonClicked();
  }

  return true;
}

bool WebauthnDialogViewImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ? model_->IsAcceptButtonEnabled()
                                        : true;
}

std::u16string WebauthnDialogViewImpl::GetWindowTitle() const {
  return model_->GetStepTitle();
}

void WebauthnDialogViewImpl::Hide() {
  // Reset controller reference if the controller has been destroyed before the
  // view being destroyed. This happens if browser window is closed when the
  // dialog is visible.
  if (controller_) {
    controller_->OnDialogClosed();
    controller_ = nullptr;
  }
  GetWidget()->Close();
}

void WebauthnDialogViewImpl::RefreshContent() {
  sheet_view_->ReInitChildViews();
  sheet_view_->InvalidateLayout();
  SetButtonLabel(ui::DIALOG_BUTTON_OK, model_->GetAcceptButtonLabel());
  SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, model_->GetCancelButtonLabel());
  DCHECK(model_->IsCancelButtonVisible());
  SetButtons(model_->IsAcceptButtonVisible()
                 ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
                 : ui::DIALOG_BUTTON_CANCEL);

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

BEGIN_METADATA(WebauthnDialogViewImpl, views::DialogDelegateView)
END_METADATA

}  // namespace autofill
