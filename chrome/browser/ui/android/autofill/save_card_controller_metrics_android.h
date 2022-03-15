// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_CONTROLLER_METRICS_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_CONTROLLER_METRICS_ANDROID_H_

#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

enum class MessageMetrics {
  kShown = 0,
  kAccepted = 1,
  kDenied = 2,
  kIgnored = 3,
  kMaxValue = kIgnored
};

enum class MessageDialogPromptMetrics {
  kAccepted = 0,
  kDenied = 1,
  kIgnored = 2,
  kMaxValue = kIgnored
};

void LogAutofillCreditCardMessageMetrics(
    MessageMetrics metric,
    bool is_uploading,
    AutofillClient::SaveCreditCardOptions options);

void LogAutofillCreditCardMessageDialogPromptMetrics(
    MessageDialogPromptMetrics metric,
    AutofillClient::SaveCreditCardOptions options,
    bool is_link_clicked);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_CONTROLLER_METRICS_ANDROID_H_
