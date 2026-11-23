// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_ATTRIBUTION_VERIFICATION_SHEET_BRIDGE_TEST_API_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_ATTRIBUTION_VERIFICATION_SHEET_BRIDGE_TEST_API_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/android/autofill/autofill_ai_attribution_verification_sheet_bridge.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"

namespace autofill {

class AutofillAiAttributionVerificationSheetBridgeTestApi {
 public:
  explicit AutofillAiAttributionVerificationSheetBridgeTestApi(
      AutofillAiAttributionVerificationSheetBridge& bridge)
      : bridge_(bridge) {}

  void SetDecisionCallback(
      AutofillClient::AutofillAiVerificationDecisionCallback callback) {
    bridge_->decision_cb_ = std::move(callback);
  }

 private:
  const raw_ref<AutofillAiAttributionVerificationSheetBridge> bridge_;
};

inline AutofillAiAttributionVerificationSheetBridgeTestApi test_api(
    AutofillAiAttributionVerificationSheetBridge& bridge) {
  return AutofillAiAttributionVerificationSheetBridgeTestApi(bridge);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_AI_ATTRIBUTION_VERIFICATION_SHEET_BRIDGE_TEST_API_H_
