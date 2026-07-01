// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac.h"

#import "base/memory/weak_ptr.h"
#import "chrome/browser/ui/autofill/autofill_popup_controller_impl_mac_test_api.h"
#import "chrome/browser/ui/autofill/autofill_suggestion_controller_test_base.h"
#import "chrome/browser/ui/autofill/test_autofill_popup_controller_autofill_client.h"
#import "chrome/browser/ui/cocoa/touchbar/web_textfield_touch_bar_controller.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "ui/gfx/geometry/rect_f.h"

@interface MockWebTextfieldTouchBarController : WebTextfieldTouchBarController
@property(nonatomic, assign) BOOL hideCalled;
@end

@implementation MockWebTextfieldTouchBarController
@synthesize hideCalled = _hideCalled;

- (void)hideCreditCardAutofillTouchBar {
  _hideCalled = YES;
}
@end

namespace autofill {
namespace {

class AutofillPopupControllerImplMacForTesting
    : public AutofillPopupControllerImplMac {
 public:
  AutofillPopupControllerImplMacForTesting(
      base::WeakPtr<AutofillExternalDelegate> external_delegate,
      content::WebContents* web_contents,
      const LocalFrameToken& frame_token,
      const gfx::RectF& element_bounds)
      : AutofillPopupControllerImplMac(
            external_delegate,
            web_contents,
            PopupControllerCommon(frame_token,
                                  element_bounds,
                                  base::i18n::UNKNOWN_DIRECTION)) {}

  ~AutofillPopupControllerImplMacForTesting() override = default;

  // Override Hide to do nothing, so we can control lifetime in tests.
  MOCK_METHOD(void, Hide, (SuggestionHidingReason reason), (override));

  void DoHide(
      SuggestionHidingReason reason = SuggestionHidingReason::kTabGone) {
    AutofillPopupControllerImplMac::Hide(reason);
  }
};

class AutofillPopupControllerImplMacTest
    : public AutofillSuggestionControllerTestBase<
          TestAutofillPopupControllerAutofillClient<
              AutofillPopupControllerImplMacForTesting>> {
 public:
  void SetUp() override { AutofillSuggestionControllerTestBase::SetUp(); }

  AutofillPopupControllerImplMacForTesting& controller() {
    return client().suggestion_controller(manager());
  }
};

// Tests that the controller hides the touch bar when it reuses the controller.
TEST_F(AutofillPopupControllerImplMacTest,
       RecycleHidesTouchBarIfNoCreditCardSuggestions) {
  ShowSuggestions(manager(), {SuggestionType::kCreditCardEntry});

  // Simulate that the touch bar was previously shown (e.g. if we had CC
  // suggestions).
  MockWebTextfieldTouchBarController* touch_bar_controller =
      [[MockWebTextfieldTouchBarController alloc] init];
  test_api(controller()).set_touch_bar_controller(touch_bar_controller);
  ASSERT_EQ(test_api(controller()).touch_bar_controller(),
            touch_bar_controller);

  // Call Show with new, non-credit-card suggestions. This should hide the
  // touch_bar_controller_.
  ShowSuggestions(manager(), {SuggestionType::kAddressEntry});

  EXPECT_TRUE(touch_bar_controller.hideCalled);
  EXPECT_EQ(test_api(controller()).touch_bar_controller(), nil);
}

}  // namespace
}  // namespace autofill
