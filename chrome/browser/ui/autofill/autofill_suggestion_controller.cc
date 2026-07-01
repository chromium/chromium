// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"

#include <utility>

#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/popup_controller_common.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/autofill/at_memory_suggestion_controller.h"
#include "chrome/browser/ui/autofill/autofill_keyboard_accessory_controller_impl.h"
#include "components/autofill/core/common/autofill_util.h"
#elif BUILDFLAG(IS_MAC)
// We cannot include the Objective-C++ header of AutofillPopupControllerImplMac
// here. This function is defined in autofill_popup_controller_impl_mac.mm.
#else
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#endif

namespace autofill {

#if BUILDFLAG(IS_MAC)
// We cannot include the Objective-C++ header of AutofillPopupControllerImplMac
// here. This function is defined in autofill_popup_controller_impl_mac.mm.
base::WeakPtr<AutofillSuggestionController>
CreateAutofillPopupControllerImplMac(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common);
#endif

// static
base::WeakPtr<AutofillSuggestionController>
AutofillSuggestionController::Create(
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    AutofillSuggestionTriggerSource trigger_source) {
#if BUILDFLAG(IS_ANDROID)
  if (IsAtMemoryTriggerSource(trigger_source)) {
    auto* controller = new AtMemorySuggestionController(
        delegate, web_contents, std::move(controller_common));
    return controller->GetWeakPtr();
  }
  auto* controller = new AutofillKeyboardAccessoryControllerImpl(
      delegate, web_contents, std::move(controller_common));
  return controller->GetWeakPtr();
#elif BUILDFLAG(IS_MAC)
  return CreateAutofillPopupControllerImplMac(delegate, web_contents,
                                              std::move(controller_common));
#else
  auto* controller = new AutofillPopupControllerImpl(
      delegate, web_contents, std::move(controller_common),
      /*parent=*/std::nullopt);
  return controller->GetWeakPtr();
#endif
}

// static
AutofillSuggestionController::UiSessionId
AutofillSuggestionController::GenerateSuggestionUiSessionId() {
  static UiSessionId::Generator generator;
  return generator.GenerateNextId();
}

// static
base::WeakPtr<AutofillSuggestionController>
AutofillSuggestionController::GetOrCreate(
    base::WeakPtr<AutofillSuggestionController> previous,
    base::WeakPtr<AutofillSuggestionDelegate> delegate,
    content::WebContents* web_contents,
    PopupControllerCommon controller_common,
    int32_t form_control_ax_id,
    AutofillSuggestionTriggerSource trigger_source) {
  if (previous &&
      previous->MayRecycle(delegate, web_contents, trigger_source)) {
    previous->Recycle(std::move(controller_common), form_control_ax_id);
    return previous;
  }

  if (previous) {
    previous->Hide(SuggestionHidingReason::kViewDestroyed);
  }

  return Create(delegate, web_contents, std::move(controller_common),
                trigger_source);
}

}  // namespace autofill
