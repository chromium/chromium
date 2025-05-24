// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"

#include <memory>
#include <optional>
#include <utility>

#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {

BrowserAutofillManagerForPopupTest::BrowserAutofillManagerForPopupTest(
    AutofillDriver* driver)
    : BrowserAutofillManager(driver) {
  test_api(*this).SetExternalDelegate(
      std::make_unique<
          ::testing::NiceMock<AutofillExternalDelegateForPopupTest>>(this));
}

BrowserAutofillManagerForPopupTest::~BrowserAutofillManagerForPopupTest() =
    default;

AutofillExternalDelegateForPopupTest&
BrowserAutofillManagerForPopupTest::external_delegate() {
  return static_cast<AutofillExternalDelegateForPopupTest&>(
      *test_api(*this).external_delegate());
}

AutofillExternalDelegateForPopupTest::AutofillExternalDelegateForPopupTest(
    BrowserAutofillManager* autofill_manager)
    : AutofillExternalDelegate(autofill_manager) {}

AutofillExternalDelegateForPopupTest::~AutofillExternalDelegateForPopupTest() =
    default;

AutofillSuggestionControllerForTest::AutofillSuggestionControllerForTest(
    base::WeakPtr<AutofillExternalDelegate> external_delegate,
    content::WebContents* web_contents,
    const gfx::RectF& element_bounds)
    : AutofillSuggestionControllerForTestBase(
          external_delegate,
          web_contents,
          PopupControllerCommon(element_bounds,
                                base::i18n::UNKNOWN_DIRECTION,
                                gfx::NativeView())
#if !BUILDFLAG(IS_ANDROID)
      // The comma has to be inside the #if or the compile fails.
      ,
      /*form_control_ax_id=*/0
#endif
      ) {
}

AutofillSuggestionControllerForTest::~AutofillSuggestionControllerForTest() =
    default;

}  // namespace autofill
