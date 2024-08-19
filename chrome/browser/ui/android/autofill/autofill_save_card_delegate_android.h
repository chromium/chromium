// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_DELEGATE_ANDROID_H_

#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

class DeviceLockBridge;

namespace content {
class WebContents;
}

namespace autofill {

// Android implementation of AutofillSaveCardDelegate which prompts user to
// setup device lock for data privacy prior to saving autofill payment methods.
class AutofillSaveCardDelegateAndroid : public AutofillSaveCardDelegate {
 public:
  AutofillSaveCardDelegateAndroid(
      absl::variant<
          payments::PaymentsAutofillClient::LocalSaveCardPromptCallback,
          payments::PaymentsAutofillClient::UploadSaveCardPromptCallback>
          callback,
      payments::PaymentsAutofillClient::SaveCreditCardOptions options,
      content::WebContents* web_contents);

  void SetDeviceLockBridgeForTesting(
      std::unique_ptr<DeviceLockBridge> device_lock_bridge);

  AutofillSaveCardDelegateAndroid(const AutofillSaveCardDelegateAndroid&) =
      delete;
  AutofillSaveCardDelegateAndroid& operator=(
      const AutofillSaveCardDelegateAndroid&) = delete;
  ~AutofillSaveCardDelegateAndroid() override;

 private:
  // Show users an explainer dialog describing why they need to set a device
  // lock and then redirects them to the Android OS device lock set up flow.
  void PromptUserToSetDeviceLock(
      payments::PaymentsAutofillClient::UserProvidedCardDetails
          user_provided_details);

  // Attempt to save card if user successfully sets a device lock, and runs
  // appropriate callbacks such as cleaning up pointers to this delegate that
  // have their lifecycle extended.
  void OnAfterDeviceLockUi(
      payments::PaymentsAutofillClient::UserProvidedCardDetails
          user_provided_details,
      bool is_device_lock_set);

  // AutofillSaveCardDelegate:
  void GatherAdditionalConsentIfApplicable(
      payments::PaymentsAutofillClient::UserProvidedCardDetails
          user_provided_details) override;

  raw_ptr<content::WebContents> web_contents_;

  // This JNI bridge helper class launches the device lock setup flow in
  // Android. `device_lock_bridge_` is created in the constructor.
  std::unique_ptr<DeviceLockBridge> device_lock_bridge_;

  base::WeakPtrFactory<AutofillSaveCardDelegateAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_CARD_DELEGATE_ANDROID_H_
