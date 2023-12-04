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

class PlusAddressCreationDialogDelegate;
class PlusAddressCreationView;

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

  // Used to validate the view behavior in browsertests.
  PlusAddressCreationView* get_view_for_testing();
  // A mechanism to avoid view entanglements, reducing the need for view
  // mocking, etc., while still allowing tests of specific business logic.
  // TODO(crbug.com/1467623): Add more end-to-end coverage as the modal behavior
  // comes fully online.
  void set_suppress_ui_for_testing(bool should_suppress);
  // Used to validate storage and clearing of `maybe_plus_profile_`.
  absl::optional<PlusProfile> get_plus_profile_for_testing();

 private:
  // WebContentsUserData:
  explicit PlusAddressCreationControllerDesktop(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      PlusAddressCreationControllerDesktop>;

  // Populates `plus_profile_` with `maybe_plus_profile` if it's not an error.
  void OnPlusAddressReserved(const PlusProfileOrError& maybe_plus_profile);
  // Autofills `plus_address` in the targeted field by running callback_.
  void OnPlusAddressConfirmed(const PlusProfileOrError& maybe_plus_profile);

  base::WeakPtr<PlusAddressCreationControllerDesktop> GetWeakPtr();

  std::unique_ptr<PlusAddressCreationDialogDelegate> dialog_delegate_;
  url::Origin relevant_origin_;
  PlusAddressCallback callback_;
  bool suppress_ui_for_testing_ = false;
  // This is set by OnPlusAddressReserved and cleared when the dialog is closed.
  absl::optional<PlusProfile> plus_profile_;

  base::WeakPtrFactory<PlusAddressCreationControllerDesktop> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_DESKTOP_H_
