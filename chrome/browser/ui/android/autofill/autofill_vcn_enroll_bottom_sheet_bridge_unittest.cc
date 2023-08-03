// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"

#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillVCNEnrollBottomSheetBridgeTest, RequestShowContent) {
  AutofillVCNEnrollBottomSheetBridge bridge;

  bool did_show =
      bridge.RequestShowContent(/*web_contents=*/nullptr, /*delegate=*/nullptr);

  EXPECT_FALSE(did_show);
}

}  // namespace autofill
