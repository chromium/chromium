// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_save_iban_delegate.h"

#include <string>

#include "base/notreached.h"
#include "components/browser_ui/device_lock/android/device_lock_bridge.h"
#include "ui/android/view_android.h"

namespace autofill {

AutofillSaveIbanDelegate::AutofillSaveIbanDelegate(
    payments::PaymentsAutofillClient::SaveIbanPromptCallback save_iban_callback,
    content::WebContents* web_contents)
    : save_iban_callback_(std::move(save_iban_callback)),
      web_contents_(web_contents),
      device_lock_bridge_(std::make_unique<DeviceLockBridge>()) {}

AutofillSaveIbanDelegate::~AutofillSaveIbanDelegate() = default;

void AutofillSaveIbanDelegate::OnUiAccepted(
    base::OnceClosure callback,
    std::u16string_view user_provided_nickname) {
  on_finished_gathering_consent_callback_ = std::move(callback);
  device_lock_bridge_->LaunchDeviceLockUiIfNeededBeforeRunningCallback(
      web_contents_->GetNativeView()->GetWindowAndroid(),
      base::BindOnce(&AutofillSaveIbanDelegate::OnAfterDeviceLockUi,
                     weak_ptr_factory_.GetWeakPtr(), user_provided_nickname));
}

void AutofillSaveIbanDelegate::OnUiCanceled() {
  std::move(save_iban_callback_)
      .Run(payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::
               kDeclined,
           /*user_provided_nickname=*/u"");
}

void AutofillSaveIbanDelegate::OnUiIgnored() {
  std::move(save_iban_callback_)
      .Run(
          payments::PaymentsAutofillClient::SaveIbanOfferUserDecision::kIgnored,
          /*user_provided_nickname=*/u"");
}

void AutofillSaveIbanDelegate::SetDeviceLockBridgeForTesting(
    std::unique_ptr<DeviceLockBridge> device_lock_bridge) {
  device_lock_bridge_ = std::move(device_lock_bridge);
}

void AutofillSaveIbanDelegate::OnAfterDeviceLockUi(
    std::u16string_view user_provided_nickname,
    bool user_decision) {
  std::move(save_iban_callback_)
      .Run(user_decision ? payments::PaymentsAutofillClient::
                               SaveIbanOfferUserDecision::kAccepted
                         : payments::PaymentsAutofillClient::
                               SaveIbanOfferUserDecision::kDeclined,
           user_decision ? user_provided_nickname : u"");
  std::move(on_finished_gathering_consent_callback_).Run();
}

}  // namespace autofill
