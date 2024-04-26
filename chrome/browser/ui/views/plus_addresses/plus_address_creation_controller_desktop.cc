// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/views/plus_addresses/plus_address_creation_dialog_delegate.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/plus_addresses/features.h"
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

void PlusAddressCreationControllerDesktop::OnRefreshClicked() {
  PlusAddressService* plus_address_service = GetPlusAddressService();
  if (!plus_address_service) {
    return;
  }
  plus_address_service->RefreshPlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerDesktop::OnPlusAddressReserved,
          GetWeakPtr()));
}

PlusAddressService*
PlusAddressCreationControllerDesktop::GetPlusAddressService() {
  return PlusAddressServiceFactory::GetForBrowserContext(
      GetWebContents().GetBrowserContext());
}

void PlusAddressCreationControllerDesktop::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  if (dialog_delegate_) {
    return;
  }
  PlusAddressService* plus_address_service = GetPlusAddressService();
  if (!plus_address_service) {
    // TODO(crbug.com/40276862): Verify expected behavior in this case and the
    // missing email case below.
    return;
  }
  std::optional<std::string> maybe_email =
      plus_address_service->GetPrimaryEmail();
  if (maybe_email == std::nullopt) {
    // TODO(b/295075403): Validate that early return is desired behavior for
    // the optional not-present case.
    return;
  }

  relevant_origin_ = main_frame_origin;
  callback_ = std::move(callback);

  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown);
  modal_shown_time_ = clock_->Now();
  if (!suppress_ui_for_testing_) {
    const bool offer_refresh =
        plus_address_service->IsRefreshingSupported(relevant_origin_) &&
        base::FeatureList::IsEnabled(
            features::kPlusAddressRefreshUiInDesktopModal);
    dialog_delegate_ = std::make_unique<PlusAddressCreationDialogDelegate>(
        GetWeakPtr(), &GetWebContents(), maybe_email.value(), offer_refresh);
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

  if (PlusAddressService* plus_address_service = GetPlusAddressService()) {
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
  // TODO(b/320541525) ModalEvent is in sync with actual user action. May
  // re-evaluate the use of this metric when modal becomes more complex.
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled);
  if (modal_error_status_.has_value()) {
    RecordModalShownDuration(modal_error_status_.value());
    modal_error_status_.reset();
  } else {
    RecordModalShownDuration(
        PlusAddressMetrics::PlusAddressModalCompletionStatus::kModalCanceled);
  }
}
void PlusAddressCreationControllerDesktop::OnDialogDestroyed() {
  dialog_delegate_.reset();
  plus_profile_.reset();
}

PlusAddressCreationView*
PlusAddressCreationControllerDesktop::get_view_for_testing() {
  return dialog_delegate_.get();
}

void PlusAddressCreationControllerDesktop::RecordModalShownDuration(
    const PlusAddressMetrics::PlusAddressModalCompletionStatus status) {
  if (modal_shown_time_.has_value()) {
    PlusAddressMetrics::RecordModalShownDuration(
        status, clock_->Now() - modal_shown_time_.value());
    modal_shown_time_.reset();
  }
}

void PlusAddressCreationControllerDesktop::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

std::optional<PlusProfile>
PlusAddressCreationControllerDesktop::get_plus_profile_for_testing() {
  return plus_profile_;
}

base::WeakPtr<PlusAddressCreationControllerDesktop>
PlusAddressCreationControllerDesktop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PlusAddressCreationControllerDesktop::OnPlusAddressReserved(
    const PlusProfileOrError& maybe_plus_profile) {
  if (maybe_plus_profile.has_value()) {
    plus_profile_ = maybe_plus_profile.value();
  } else {
    modal_error_status_ = PlusAddressMetrics::PlusAddressModalCompletionStatus::
        kReservePlusAddressError;
  }
  // Display result on UI only after setting `plus_profile_` to prevent
  // premature confirm without `plus_profile_` value.
  if (dialog_delegate_) {
    if (PlusAddressService* service = GetPlusAddressService();
        !service || !service->IsRefreshingSupported(relevant_origin_)) {
      dialog_delegate_->HideRefreshButton();
    }
    dialog_delegate_->ShowReserveResult(maybe_plus_profile);
  }
}

void PlusAddressCreationControllerDesktop::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  if (maybe_plus_profile.has_value()) {
    std::move(callback_).Run(maybe_plus_profile->plus_address);
    // PlusAddress successfully confirmed, closing the modal.
    RecordModalShownDuration(
        PlusAddressMetrics::PlusAddressModalCompletionStatus::kModalConfirmed);
  } else {
    modal_error_status_ = PlusAddressMetrics::PlusAddressModalCompletionStatus::
        kConfirmPlusAddressError;
  }

  // Display result on UI after setting `modal_error_status_` to ensure correct
  // metric is recorded.
  if (dialog_delegate_) {
    dialog_delegate_->ShowConfirmResult(maybe_plus_profile);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerDesktop);
}  // namespace plus_addresses
