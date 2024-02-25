// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"

#include <optional>

#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"

namespace plus_addresses {
// static
PlusAddressCreationController* PlusAddressCreationController::GetOrCreate(
    content::WebContents* web_contents) {
  PlusAddressCreationControllerAndroid::CreateForWebContents(web_contents);
  return PlusAddressCreationControllerAndroid::FromWebContents(web_contents);
}

PlusAddressCreationControllerAndroid::PlusAddressCreationControllerAndroid(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PlusAddressCreationControllerAndroid>(
          *web_contents) {}
PlusAddressCreationControllerAndroid::~PlusAddressCreationControllerAndroid() =
    default;
void PlusAddressCreationControllerAndroid::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  if (view_) {
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
  std::optional<std::string> maybe_email =
      plus_address_service->GetPrimaryEmail();
  if (maybe_email == std::nullopt) {
    return;
  }

  callback_ = std::move(callback);
  relevant_origin_ = main_frame_origin;
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown);
  modal_shown_time_ = clock_->Now();
  if (!suppress_ui_for_testing_) {
    view_ = std::make_unique<PlusAddressCreationViewAndroid>(GetWeakPtr(),
                                                             &GetWebContents());
    view_->ShowInit(maybe_email.value());
  }
  plus_address_service->ReservePlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerAndroid::OnPlusAddressReserved,
          GetWeakPtr()));
}

void PlusAddressCreationControllerAndroid::OnConfirmed() {
  CHECK(plus_profile_.has_value());
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed);
  if (plus_profile_->is_confirmed) {
    OnPlusAddressConfirmed(plus_profile_.value());
    return;
  }
  PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  if (plus_address_service) {
    // Note: this call may fail if this modal is confirmed on the same
    // `relevant_origin_` from another device.
    plus_address_service->ConfirmPlusAddress(
        relevant_origin_, plus_profile_->plus_address,
        base::BindOnce(
            &PlusAddressCreationControllerAndroid::OnPlusAddressConfirmed,
            GetWeakPtr()));
  }
}

void PlusAddressCreationControllerAndroid::OnCanceled() {
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

void PlusAddressCreationControllerAndroid::OnDialogDestroyed() {
  view_.reset();
  plus_profile_.reset();
}

void PlusAddressCreationControllerAndroid::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

std::optional<PlusProfile>
PlusAddressCreationControllerAndroid::get_plus_profile_for_testing() {
  return plus_profile_;
}

void PlusAddressCreationControllerAndroid::OnPlusAddressReserved(
    const PlusProfileOrError& maybe_plus_profile) {
  // Note that in case of `suppress_ui_for_testing_` or bottom sheet dismissal
  // prior to service response, `view_` will be null.
  if (view_) {
    view_->ShowReserveResult(maybe_plus_profile);
  }
  if (maybe_plus_profile.has_value()) {
    plus_profile_ = maybe_plus_profile.value();
  } else {
    modal_error_status_ = PlusAddressMetrics::PlusAddressModalCompletionStatus::
        kReservePlusAddressError;
  }
}

void PlusAddressCreationControllerAndroid::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  // Note that in case of `suppress_ui_for_testing_` or bottom sheet dismissal
  // prior to service response, `view_` will be null.
  if (view_) {
    view_->ShowConfirmResult(maybe_plus_profile);
  }
  if (maybe_plus_profile.has_value()) {
    std::move(callback_).Run(maybe_plus_profile->plus_address);
    RecordModalShownDuration(
        PlusAddressMetrics::PlusAddressModalCompletionStatus::kModalConfirmed);
  } else {
    modal_error_status_ = PlusAddressMetrics::PlusAddressModalCompletionStatus::
        kConfirmPlusAddressError;
  }
}

void PlusAddressCreationControllerAndroid::RecordModalShownDuration(
    const PlusAddressMetrics::PlusAddressModalCompletionStatus status) {
  if (modal_shown_time_.has_value()) {
    PlusAddressMetrics::RecordModalShownDuration(
        status, clock_->Now() - modal_shown_time_.value());
    modal_shown_time_.reset();
  }
}

base::WeakPtr<PlusAddressCreationControllerAndroid>
PlusAddressCreationControllerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerAndroid);
}  // namespace plus_addresses
