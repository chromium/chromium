// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {
// static
PlusAddressCreationController* PlusAddressCreationController::GetOrCreate(
    content::WebContents* web_contents) {
  PlusAddressCreationControllerDesktop::CreateForWebContents(web_contents);
  return PlusAddressCreationControllerDesktop::FromWebContents(web_contents);
}
PlusAddressCreationControllerDesktop::PlusAddressCreationControllerDesktop(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PlusAddressCreationControllerDesktop>(
          *web_contents) {}

PlusAddressCreationControllerDesktop::~PlusAddressCreationControllerDesktop() {
  // If the dialog is still open, ensure it gets cleaned up.
  if (dialog_delegate_ && dialog_delegate_->GetWidget()) {
    dialog_delegate_->GetWidget()->CloseNow();
  }
}

void PlusAddressCreationControllerDesktop::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  if (dialog_delegate_) {
    return;
  }
  PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  if (!plus_address_service) {
    // TODO(crbug.com/1467623): Verify expected behavior in this case and the
    // missing email case below.
    return;
  }
  absl::optional<std::string> maybe_email =
      plus_address_service->GetPrimaryEmail();
  if (maybe_email == absl::nullopt) {
    // TODO(b/295075403): Validate that early return is desired behavior for
    // the optional not-present case.
    return;
  }

  relevant_origin_ = main_frame_origin;
  callback_ = std::move(callback);

  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown);
  if (!suppress_ui_for_testing_) {
    dialog_delegate_ = std::make_unique<PlusAddressCreationDialogDelegate>(
        GetWeakPtr(), &GetWebContents(), maybe_email.value());
    constrained_window::ShowWebModalDialogViews(dialog_delegate_.get(),
                                                &GetWebContents());
  }

  plus_address_service->ReservePlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerDesktop::OnPlusAddressReserved,
          GetWeakPtr()));
}

void PlusAddressCreationControllerDesktop::OnConfirmed() {
  // The UI prevents any attempt to Confirm if Reserve() had failed.
  CHECK(plus_profile_.has_value());
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed);

  if (plus_profile_->is_confirmed) {
    OnPlusAddressConfirmed(plus_profile_.value());
    return;
  }

  if (PlusAddressService* plus_address_service =
          PlusAddressServiceFactory::GetForBrowserContext(
              GetWebContents().GetBrowserContext());
      plus_address_service) {
    // Note: this call may fail if this modal is confirmed on the same
    // `relevant_origin_` from another device.
    plus_address_service->ConfirmPlusAddress(
        relevant_origin_, plus_profile_->plus_address,
        base::BindOnce(
            &PlusAddressCreationControllerDesktop::OnPlusAddressConfirmed,
            GetWeakPtr()));
  }
}
void PlusAddressCreationControllerDesktop::OnCanceled() {
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled);
}
void PlusAddressCreationControllerDesktop::OnDialogDestroyed() {
  dialog_delegate_.reset();
  plus_profile_.reset();
}

PlusAddressCreationView*
PlusAddressCreationControllerDesktop::get_view_for_testing() {
  return dialog_delegate_.get();
}

void PlusAddressCreationControllerDesktop::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

absl::optional<PlusProfile>
PlusAddressCreationControllerDesktop::get_plus_profile_for_testing() {
  return plus_profile_;
}

base::WeakPtr<PlusAddressCreationControllerDesktop>
PlusAddressCreationControllerDesktop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PlusAddressCreationControllerDesktop::OnPlusAddressReserved(
    const PlusProfileOrError& maybe_plus_profile) {
  if (dialog_delegate_) {
    dialog_delegate_->ShowReserveResult(maybe_plus_profile);
  }
  if (maybe_plus_profile.has_value()) {
    plus_profile_ = maybe_plus_profile.value();
  }
}

void PlusAddressCreationControllerDesktop::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  if (dialog_delegate_) {
    dialog_delegate_->ShowConfirmResult(maybe_plus_profile);
  }
  if (maybe_plus_profile.has_value()) {
    std::move(callback_).Run(maybe_plus_profile->plus_address);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerDesktop);
}  // namespace plus_addresses
