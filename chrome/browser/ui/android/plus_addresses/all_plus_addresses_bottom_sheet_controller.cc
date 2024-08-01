// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_controller.h"

#include "base/check_deref.h"
#include "chrome/browser/plus_addresses/plus_address_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/android/plus_addresses/all_plus_addresses_bottom_sheet_view.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/browser/web_contents.h"

namespace plus_addresses {

AllPlusAddressesBottomSheetController::AllPlusAddressesBottomSheetController(
    content::WebContents* web_contents)
    : web_contents_(CHECK_DEREF(web_contents)),
      view_(std::make_unique<AllPlusAddressesBottomSheetView>(this)) {}

AllPlusAddressesBottomSheetController::
    ~AllPlusAddressesBottomSheetController() = default;

void AllPlusAddressesBottomSheetController::Show(
    SelectPlusAddressCallback on_plus_address_selected) {
  if (on_plus_address_selected_) {
    return;
  }
  on_plus_address_selected_ = std::move(on_plus_address_selected);
  plus_addresses::PlusAddressService* service =
      PlusAddressServiceFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  view_->Show(service->GetPlusProfiles());
}

void AllPlusAddressesBottomSheetController::OnPlusAddressSelected(
    const std::string& plus_address) {
  std::move(on_plus_address_selected_).Run(base::optional_ref(plus_address));
}

void AllPlusAddressesBottomSheetController::OnBottomSheetDismissed() {
  std::move(on_plus_address_selected_).Run(std::nullopt);
}

gfx::NativeView AllPlusAddressesBottomSheetController::GetNativeView() {
  return web_contents_->GetNativeView();
}

Profile* AllPlusAddressesBottomSheetController::GetProfile() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext());
}

}  // namespace plus_addresses
