// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"

#include <memory>
#include <optional>
#include <utility>

#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"

namespace autofill {

BrowserAutofillManagerForPopupTest::BrowserAutofillManagerForPopupTest(
    AutofillDriver* driver)
    : BrowserAutofillManager(driver, "en-US") {
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
    const gfx::RectF& element_bounds
#if BUILDFLAG(IS_ANDROID)
    ,
    ShowPasswordMigrationWarningCallback show_pwd_migration_warning_callback
#endif
    )
    : AutofillSuggestionControllerForTestBase(
          external_delegate,
          web_contents,
          PopupControllerCommon(element_bounds,
                                base::i18n::UNKNOWN_DIRECTION,
                                nullptr),
#if !BUILDFLAG(IS_ANDROID)
          /*form_control_ax_id=*/0
#else
          std::move(show_pwd_migration_warning_callback)
#endif
      ) {
}

AutofillSuggestionControllerForTest::~AutofillSuggestionControllerForTest() =
    default;

}  // namespace autofill
