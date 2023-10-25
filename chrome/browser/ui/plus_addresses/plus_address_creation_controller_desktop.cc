// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_desktop.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_dialog_view.h"
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

PlusAddressCreationControllerDesktop::~PlusAddressCreationControllerDesktop() =
    default;
void PlusAddressCreationControllerDesktop::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  if (ui_modal_showing_) {
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

  plus_address_service->ReservePlusAddress(
      relevant_origin_,
      base::BindOnce(
          &PlusAddressCreationControllerDesktop::OnPlusAddressReserved,
          GetWeakPtr(), maybe_email.value()));
}

void PlusAddressCreationControllerDesktop::OnConfirmed() {
  PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed);
  if (plus_address_service) {
    plus_address_service->ConfirmPlusAddress(
        relevant_origin_, plus_address_,
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
  ui_modal_showing_ = false;
}

void PlusAddressCreationControllerDesktop::set_suppress_ui_for_testing(
    bool should_suppress) {
  suppress_ui_for_testing_ = should_suppress;
}

base::WeakPtr<PlusAddressCreationControllerDesktop>
PlusAddressCreationControllerDesktop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void PlusAddressCreationControllerDesktop::OnPlusAddressReserved(
    const std::string& primary_email_address,
    const PlusProfileOrError& maybe_plus_profile) {
  if (!maybe_plus_profile.has_value()) {
    // TODO: crbug.com/1467623 - Handle error case.
    return;
  }
  plus_address_ = maybe_plus_profile->plus_address;
  // Autofill the callback and save `plus_address` in the service.
  if (maybe_plus_profile->is_confirmed) {
    // TODO: crbug.com/1467623 - Add metrics here?
    std::move(callback_).Run(maybe_plus_profile->plus_address);
    return;
  }
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalShown);
  if (!suppress_ui_for_testing_) {
    ShowPlusAddressCreationDialogView(&GetWebContents(), GetWeakPtr(),
                                      primary_email_address,
                                      maybe_plus_profile->plus_address);
    ui_modal_showing_ = true;
  }
}

void PlusAddressCreationControllerDesktop::OnPlusAddressConfirmed(
    const PlusProfileOrError& maybe_plus_profile) {
  if (!maybe_plus_profile.has_value()) {
    // TODO: crbug.com/1467623 - Handle error case.
    return;
  }
  std::move(callback_).Run(maybe_plus_profile->plus_address);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerDesktop);
}  // namespace plus_addresses
