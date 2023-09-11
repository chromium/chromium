// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_ANDROID_H_

#include "chrome/browser/ui/android/plus_addresses/plus_address_creation_view_android.h"
#include "chrome/browser/ui/plus_addresses/plus_address_creation_controller.h"
#include "components/plus_addresses/plus_address_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/origin.h"

namespace plus_addresses {

// A (hopefully) temporary Android-specific implementation of
// `PlusAddressCreationController`. The hope is to converge this back to a
// single implementation for desktop and android, but while both are in
// development, a split is convenient.
class PlusAddressCreationControllerAndroid
    : public PlusAddressCreationController,
      public content::WebContentsUserData<
          PlusAddressCreationControllerAndroid> {
 public:
  ~PlusAddressCreationControllerAndroid() override;

  // PlusAddressCreationController implementation:
  void OfferCreation(const url::Origin& main_frame_origin,
                     PlusAddressCallback callback) override;
  void OnConfirmed() override;
  void OnCanceled() override;
  void OnDialogDestroyed() override;

  // A mechanism to avoid view entanglements, reducing the need for JNI mocking,
  // etc., while still allowing tests of specific business logic.
  // TODO(crbug.com/1467623): Add end-to-end coverage as the modal behavior
  // comes fully online.
  void set_suppress_ui_for_testing(bool should_suppress);

 private:
  // WebContentsUserData:
  explicit PlusAddressCreationControllerAndroid(
      content::WebContents* web_contents);
  friend class content::WebContentsUserData<
      PlusAddressCreationControllerAndroid>;

  base::WeakPtr<PlusAddressCreationControllerAndroid> GetWeakPtr();

  std::unique_ptr<PlusAddressCreationViewAndroid> view_;
  url::Origin relevant_origin_;
  PlusAddressCallback callback_;
  bool suppress_ui_for_testing_ = false;

  base::WeakPtrFactory<PlusAddressCreationControllerAndroid> weak_ptr_factory_{
      this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace plus_addresses

#endif  // CHROME_BROWSER_UI_ANDROID_PLUS_ADDRESSES_PLUS_ADDRESS_CREATION_CONTROLLER_ANDROID_H_
