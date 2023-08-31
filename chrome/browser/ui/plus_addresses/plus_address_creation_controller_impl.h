// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_IMPL_H_

#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace plus_addresses {

class PlusAddressCreationControllerImpl
    : public PlusAddressCreationController,
      public content::WebContentsUserData<PlusAddressCreationControllerImpl> {
 public:
  ~PlusAddressCreationControllerImpl() override;

  // PlusAddressCreationController implementation:
  void OfferCreation(const url::Origin& main_frame_origin,
                     PlusAddressCallback callback) override;
  void OnConfirmed() override;
  void OnCanceled() override;
  void OnDialogDestroyed() override;

 private:
  // WebContentsUserData:
  explicit PlusAddressCreationControllerImpl(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<PlusAddressCreationControllerImpl>;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_IMPL_H_
