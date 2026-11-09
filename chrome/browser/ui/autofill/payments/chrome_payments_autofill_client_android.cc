// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/payments/chrome_payments_autofill_client_android.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/android/autofill/autofill_wallet_reminder_notice_bottom_sheet_bridge.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace autofill::payments {

// static
std::unique_ptr<ChromePaymentsAutofillClient>
ChromePaymentsAutofillClient::Create(ContentAutofillClient* client) {
  return std::make_unique<ChromePaymentsAutofillClientAndroid>(client);
}

ChromePaymentsAutofillClientAndroid::ChromePaymentsAutofillClientAndroid(
    ContentAutofillClient* client)
    : ChromePaymentsAutofillClient(client) {}

ChromePaymentsAutofillClientAndroid::~ChromePaymentsAutofillClientAndroid() =
    default;

void ChromePaymentsAutofillClientAndroid::ShowWalletReminderNotice(
    LegalMessageLines legal_message_lines) {
  if (auto* bridge = GetOrCreateWalletReminderNoticeBridge()) {
    bridge->RequestShowContent(std::move(legal_message_lines));
  }
}

AutofillWalletReminderNoticeBottomSheetBridge*
ChromePaymentsAutofillClientAndroid::GetOrCreateWalletReminderNoticeBridge() {
  if (!autofill_wallet_reminder_notice_bottom_sheet_bridge_) {
    if (!web_contents()) {
      return nullptr;
    }
    if (auto* window_android = web_contents()->GetTopLevelNativeWindow()) {
      autofill_wallet_reminder_notice_bottom_sheet_bridge_ =
          std::make_unique<AutofillWalletReminderNoticeBottomSheetBridge>(
              window_android);
    }
  }
  return autofill_wallet_reminder_notice_bottom_sheet_bridge_.get();
}

}  // namespace autofill::payments
