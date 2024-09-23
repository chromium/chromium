// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"

#import <Cocoa/Cocoa.h>

#include <optional>
#include <string>
#include <vector>

#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/mock_autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/scoped_default_font_description.h"

namespace autofill {

namespace {

NSString* const kCreditCardAutofillTouchBarId = @"credit-card-autofill";
NSString* const kCreditCardItemsTouchId = @"CREDIT-CARD-ITEMS";

}  // namespace

class CreditCardAutofillTouchBarControllerUnitTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();

    touch_bar_controller_ = [[CreditCardAutofillTouchBarController alloc]
        initWithController:&autofill_popup_controller_];
  }

  void SetSuggestions(std::vector<Suggestion> suggestions) {
    autofill_popup_controller_.set_suggestions(std::move(suggestions));
  }

  void SetSuggestions(const std::vector<autofill::SuggestionType>& types) {
    std::vector<Suggestion> suggestions;
    suggestions.reserve(types.size());
    for (autofill::SuggestionType type : types) {
      suggestions.emplace_back("", "", Suggestion::Icon::kNoIcon, type);
    }
    SetSuggestions(std::move(suggestions));
  }

  CreditCardAutofillTouchBarController* __strong touch_bar_controller_;

 private:
  MockAutofillPopupController autofill_popup_controller_;
};

// Tests to check if the touch bar shows up properly.
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, TouchBar) {
  // Touch bar shouldn't appear if the popup is not for credit cards.
  [touch_bar_controller_ setIsCreditCardPopup:false];
  EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

  // Touch bar shouldn't appear if the popup is empty.
  [touch_bar_controller_ setIsCreditCardPopup:true];
  EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetSuggestions(
      {SuggestionType::kCreditCardEntry, SuggestionType::kCreditCardEntry});
  NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
  EXPECT_TRUE(touch_bar);
  EXPECT_TRUE([[touch_bar customizationIdentifier]
      isEqual:ui::GetTouchBarId(kCreditCardAutofillTouchBarId)]);
  EXPECT_EQ(1UL, [[touch_bar itemIdentifiers] count]);
}

// Tests to check that the touch bar doesn't show more than 3 items
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, TouchBarCardLimit) {
  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetSuggestions(
      {SuggestionType::kCreditCardEntry, SuggestionType::kCreditCardEntry,
       SuggestionType::kCreditCardEntry, SuggestionType::kCreditCardEntry});
  NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
  EXPECT_TRUE(touch_bar);
  EXPECT_TRUE([[touch_bar customizationIdentifier]
      isEqual:ui::GetTouchBarId(kCreditCardAutofillTouchBarId)]);

  NSTouchBarItem* item = [touch_bar_controller_
                   touchBar:touch_bar
      makeItemForIdentifier:ui::GetTouchBarItemId(kCreditCardAutofillTouchBarId,
                                                  kCreditCardItemsTouchId)];
  NSGroupTouchBarItem* groupItem = static_cast<NSGroupTouchBarItem*>(item);

  EXPECT_EQ(3UL, [[[groupItem groupTouchBar] itemIdentifiers] count]);
}

// Tests for for the credit card button.
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, CreditCardButtonCheck) {
  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetSuggestions(
      {Suggestion("bufflehead", "canvasback", Suggestion::Icon::kNoIcon,
                  SuggestionType::kCreditCardEntry)});
  NSButton* button = [touch_bar_controller_ createCreditCardButtonAtRow:0];
  EXPECT_TRUE(button);
  EXPECT_EQ(0, [button tag]);
  EXPECT_EQ("bufflehead canvasback", base::SysNSStringToUTF8([button title]));
}

}  // namespace autofill
