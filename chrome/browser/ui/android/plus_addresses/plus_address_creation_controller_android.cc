// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_controller_android.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  if (!view_) {
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
      return;
    }

    callback_ = std::move(callback);
    relevant_origin_ = main_frame_origin;
    plus_address_service->ReservePlusAddress(
        relevant_origin_,
        base::BindOnce(
            &PlusAddressCreationControllerAndroid::OnPlusAddressReserved,
            GetWeakPtr(), maybe_email.value()));
  }
}

void PlusAddressCreationControllerAndroid::OnConfirmed() {
  if (!plus_profile_.has_value()) {
    return;
  }
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
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalCanceled);
}

void PlusAddressCreationControllerAndroid::OnDialogDestroyed() {
  view_.reset();
  plus_profile_.reset();
}

void PlusAddressCreationControllerAndroid::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

absl::optional<PlusProfile>
PlusAddressCreationControllerAndroid::get_plus_profile_for_testing() {
  return plus_profile_;
}

void PlusAddressCreationControllerAndroid::OnPlusAddressReserved(
    const std::string& primary_email_address,
    const PlusProfileOrError& maybe_plus_profile) {
  if (!maybe_plus_profile.has_value()) {
    // TODO: crbug.com/1467623 - Handle error case.
    return;
  }
  plus_profile_ = maybe_plus_profile.value();
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown);
  if (!suppress_ui_for_testing_ && !view_) {
    view_ = std::make_unique<PlusAddressCreationViewAndroid>(GetWeakPtr(),
                                                             &GetWebContents());
    view_->Show(primary_email_address, plus_profile_->plus_address);
  }
}

void PlusAddressCreationControllerAndroid::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  if (!maybe_plus_profile.has_value()) {
    // TODO: crbug.com/1467623 - Handle error case.
    return;
  }
  std::move(callback_).Run(maybe_plus_profile->plus_address);
}

base::WeakPtr<PlusAddressCreationControllerAndroid>
PlusAddressCreationControllerAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerAndroid);
}  // namespace plus_addresses
