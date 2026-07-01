// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"

#include <memory>
#include <optional>
#include <utility>

#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/native_ui_types.h"
#include "url/gurl.h"

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
    const LocalFrameToken& frame_token,
    const gfx::RectF& element_bounds)
    : AutofillSuggestionControllerForTestBase(
          external_delegate,
          web_contents,
          PopupControllerCommon(frame_token,
                                element_bounds,
                                base::i18n::UNKNOWN_DIRECTION)) {}

AutofillSuggestionControllerForTest::~AutofillSuggestionControllerForTest() =
    default;

}  // namespace autofill
