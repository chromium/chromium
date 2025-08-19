// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_BNPL_UI_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_BNPL_UI_DELEGATE_H_

#include "components/autofill/core/browser/ui/payments/bnpl_ui_delegate.h"

namespace autofill::payments {

// Android implementation of the `BnplUiDelegate` interface. This class handles
// the UI for the BNPL autofill flow on the Android platform.
class AndroidBnplUiDelegate : public BnplUiDelegate {
 public:
  AndroidBnplUiDelegate();
  AndroidBnplUiDelegate(const AndroidBnplUiDelegate& other) = delete;
  AndroidBnplUiDelegate& operator=(const AndroidBnplUiDelegate& other) = delete;
  ~AndroidBnplUiDelegate() override;
};

}  // namespace autofill::payments

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_ANDROID_BNPL_UI_DELEGATE_H_
