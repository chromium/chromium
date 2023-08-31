// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller_impl.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "components/plus_addresses/plus_address_service.h"

namespace plus_addresses {
// static
PlusAddressCreationController* PlusAddressCreationController::GetOrCreate(
    content::WebContents* web_contents) {
  PlusAddressCreationControllerImpl::CreateForWebContents(web_contents);
  return PlusAddressCreationControllerImpl::FromWebContents(web_contents);
}
PlusAddressCreationControllerImpl::PlusAddressCreationControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<PlusAddressCreationControllerImpl>(
          *web_contents) {}
PlusAddressCreationControllerImpl::~PlusAddressCreationControllerImpl() =
    default;
void PlusAddressCreationControllerImpl::OfferCreation(
    const url::Origin& main_frame_origin,
    PlusAddressCallback callback) {
  // TODO(crbug.com/1467623): implement modal flows.
  // This function, in the future, will:
  // * Run a static creation function to get a platform-specific UI view.
  // * Show the modal UI.
  // * Take callbacks on confirmation, cancel, etc. from the UI view.
  // * Note that view lifecycles on desktop and android may differ slightly. By
  //   hiding the creation of this class on a static creation function, we could
  //   also make separate controllers for android vs desktop platforms.
  PlusAddressService* plus_address_service =
      PlusAddressServiceFactory::GetForBrowserContext(
          GetWebContents().GetBrowserContext());
  if (plus_address_service) {
    plus_address_service->OfferPlusAddressCreation(main_frame_origin,
                                                   std::move(callback));
  }
}

// TODO(crbug.com/1467623): implement modal flows.
void PlusAddressCreationControllerImpl::OnConfirmed() {}
void PlusAddressCreationControllerImpl::OnCanceled() {}
void PlusAddressCreationControllerImpl::OnDialogDestroyed() {}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PlusAddressCreationControllerImpl);
}  // namespace plus_addresses
