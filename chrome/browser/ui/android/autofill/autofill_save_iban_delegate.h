// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_IBAN_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_IBAN_DELEGATE_H_

#include <string>
#include <string_view>

#include "components/autofill/core/browser/autofill_client.h"
#include "content/public/browser/web_contents.h"

class DeviceLockBridge;

namespace autofill {

// Delegate class providing callbacks for UIs presenting save IBAN offers.
class AutofillSaveIbanDelegate {
 public:
  explicit AutofillSaveIbanDelegate(
      AutofillClient::SaveIbanPromptCallback save_iban_callback,
      content::WebContents* web_contents);

  ~AutofillSaveIbanDelegate();

  void OnUiShown();
  // Callback `on_save_iban_completed` will be triggered after the save IBAN
  // flow has finished.
  void OnUiAccepted(base::OnceClosure on_save_iban_completed,
                    std::u16string_view user_provided_nickname = u"");
  void OnUiCanceled();
  void OnUiIgnored();

  void SetDeviceLockBridgeForTesting(
      std::unique_ptr<DeviceLockBridge> device_lock_bridge);

 protected:
  // Called when all of the prerequisites for saving a IBAN have been met.
  void OnFinishedGatheringConsent(
      const AutofillClient::SaveIbanOfferUserDecision& user_decision,
      std::u16string_view user_provided_nickname = u"");

 private:
  // This function by default saves the IBAN, but allows subclasses to
  // override if there are prerequisites to saving the IBAN (ex: Android
  // automotive requires a pin/password to be set on the device and must
  // redirect to that flow before saving IBAN information).
  void GatherAdditionalConsentIfApplicable(
      std::u16string_view user_provided_nickname);

  // Shows users an explainer dialog describing why they need to set a device
  // lock and then redirects them to the Android OS device lock setup flow.
  void PromptUserToSetDeviceLock(std::u16string_view user_provided_nickname);

  // Attempt to save IBAN if user successfully sets a device lock, and runs
  // appropriate callbacks such as cleaning up pointers to this delegate that
  // have their lifecycle extended.
  void OnAfterDeviceLockUi(std::u16string_view user_provided_nickname,
                           bool user_decision);

  // The callback to run once the user makes a decision with respect to the
  // IBAN offer-to-save prompt.
  AutofillClient::SaveIbanPromptCallback save_iban_callback_;

  // Callback to run immediately after `save_iban_callback_`. An example of a
  // callback is cleaning up pointers to delegates that have their lifecycle
  // extended due to user going through device lock setup flows before saving a
  // IBAN.
  base::OnceClosure on_finished_gathering_consent_callback_;

  raw_ptr<content::WebContents> web_contents_;

  // This JNI bridge helper class launches the device lock setup flow in
  // Android.
  std::unique_ptr<DeviceLockBridge> device_lock_bridge_;

  base::WeakPtrFactory<AutofillSaveIbanDelegate> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_SAVE_IBAN_DELEGATE_H_
