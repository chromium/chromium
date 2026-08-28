// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/wallet_reminder_notice_ui_delegate_android.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/ui/android/autofill/autofill_wallet_reminder_notice_bottom_sheet_bridge.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace autofill::payments {

WalletReminderNoticeUiDelegateAndroid::WalletReminderNoticeUiDelegateAndroid(
    ContentAutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

WalletReminderNoticeUiDelegateAndroid::
    ~WalletReminderNoticeUiDelegateAndroid() = default;

void WalletReminderNoticeUiDelegateAndroid::ShowWalletReminderNotice(
    LegalMessageLines legal_message_lines) {
  if (!autofill_wallet_reminder_notice_bottom_sheet_bridge_) {
    if (auto* window_android =
            client_->GetWebContents().GetTopLevelNativeWindow()) {
      autofill_wallet_reminder_notice_bottom_sheet_bridge_ =
          std::make_unique<AutofillWalletReminderNoticeBottomSheetBridge>(
              window_android);
    }
  }
  if (autofill_wallet_reminder_notice_bottom_sheet_bridge_) {
    autofill_wallet_reminder_notice_bottom_sheet_bridge_->RequestShowContent(
        std::move(legal_message_lines));
  }
}

void WalletReminderNoticeUiDelegateAndroid::
    SetAutofillWalletReminderNoticeBottomSheetBridgeForTesting(
        std::unique_ptr<AutofillWalletReminderNoticeBottomSheetBridge> bridge) {
  autofill_wallet_reminder_notice_bottom_sheet_bridge_ = std::move(bridge);
}

}  // namespace autofill::payments
