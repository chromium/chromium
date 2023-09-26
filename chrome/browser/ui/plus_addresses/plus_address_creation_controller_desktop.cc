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
  if (!ui_modal_showing_) {
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
      ShowPlusAddressCreationDialogView(&GetWebContents(), GetWeakPtr(),
                                        maybe_email.value());
      ui_modal_showing_ = true;
    }
  }
}

void PlusAddressCreationControllerDesktop::OnConfirmed() {
  PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  PlusAddressMetrics::RecordModalEvent(
      PlusAddressMetrics::PlusAddressModalEvent::kModalConfirmed);
  if (plus_address_service) {
    plus_address_service->OfferPlusAddressCreation(relevant_origin_,
                                                   std::move(callback_));
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

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerDesktop);
}  // namespace plus_addresses
