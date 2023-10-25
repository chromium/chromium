// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_DESKTOP_H_

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "components/plus_addresses/plus_address_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace plus_addresses {

class PlusAddressCreationControllerDesktop
    : public PlusAddressCreationController,
      public content::WebContentsUserData<
          PlusAddressCreationControllerDesktop> {
 public:
  ~PlusAddressCreationControllerDesktop() override;

  // PlusAddressCreationController implementation:
  void OfferCreation(const url::Origin& main_frame_origin,
                     PlusAddressCallback callback) override;
  void OnConfirmed() override;
  void OnCanceled() override;
  void OnDialogDestroyed() override;

  // A mechanism to avoid view entanglements, reducing the need for view
  // mocking, etc., while still allowing tests of specific business logic.
  // TODO(crbug.com/1467623): Add more end-to-end coverage as the modal behavior
  // comes fully online.
  void set_suppress_ui_for_testing(bool should_suppress);

 private:
  // WebContentsUserData:
  explicit PlusAddressCreationControllerDesktop(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      PlusAddressCreationControllerDesktop>;

  // When `maybe_plus_profile` is a PlusProfile `profile`:
  // - if `profile.is_confirmed` is true, closes the modal & autofills
  //   `profile.plus_address`.
  // - if not, stores the reserved plus address and shows a the modal with
  //   `primary_email_address` and profile.plus_address.
  // TODO(crbug.com/1467623): Handle case where `maybe_plus_profile` is error.
  void OnPlusAddressReserved(const std::string& primary_email_address,
                             const PlusProfileOrError& maybe_plus_profile);
  // Autofills `plus_address` in the targeted field by running callback_.
  void OnPlusAddressConfirmed(const PlusProfileOrError& maybe_plus_profile);

  base::WeakPtr<PlusAddressCreationControllerDesktop> GetWeakPtr();

  url::Origin relevant_origin_;
  std::string plus_address_;
  PlusAddressCallback callback_;
  bool ui_modal_showing_ = false;
  bool suppress_ui_for_testing_ = false;

  base::WeakPtrFactory<PlusAddressCreationControllerDesktop> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_DESKTOP_H_
