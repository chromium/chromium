// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_TEST_API_H_

#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_enroll_bubble_controller_impl.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_vcn_enroll_bottom_sheet_bridge.h"
#endif

namespace autofill {

class VirtualCardEnrollBubbleControllerImplTestApi {
 public:
  explicit VirtualCardEnrollBubbleControllerImplTestApi(
      VirtualCardEnrollBubbleControllerImpl& controller)
      : controller_(controller) {}

  ~VirtualCardEnrollBubbleControllerImplTestApi() = default;

  void SetBubbleShownClosure(
      base::RepeatingClosure bubble_shown_closure_for_testing) {
    controller_->bubble_shown_closure_for_testing_ =
        bubble_shown_closure_for_testing;
  }

  void SetUiModel(std::unique_ptr<VirtualCardEnrollUiModel> ui_model) {
    controller_->ui_model_ = std::move(ui_model);
  }

#if BUILDFLAG(IS_ANDROID)
  void SetAutofillVCNEnrollBottomSheetBridge(
      std::unique_ptr<AutofillVCNEnrollBottomSheetBridge> bridge) {
    controller_->autofill_vcn_enroll_bottom_sheet_bridge_ = std::move(bridge);
  }

  bool DidShowBottomSheet() {
    return !!controller_->autofill_vcn_enroll_bottom_sheet_bridge_;
  }
#else   // !BUILDFLAG(IS_ANDROID)
  VirtualCardEnrollBubbleControllerImpl::EnrollmentStatus
  GetEnrollmentStatus() {
    return controller_->enrollment_status_;
  }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  raw_ref<VirtualCardEnrollBubbleControllerImpl> controller_;
};

inline VirtualCardEnrollBubbleControllerImplTestApi test_api(
    VirtualCardEnrollBubbleControllerImpl& controller) {
  return VirtualCardEnrollBubbleControllerImplTestApi(controller);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_PAYMENTS_VIRTUAL_CARD_ENROLL_BUBBLE_CONTROLLER_IMPL_TEST_API_H_
