// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/test/scoped_default_font_description.h"

namespace {

NSString* const kCreditCardAutofillTouchBarId = @"credit-card-autofill";
NSString* const kCreditCardItemsTouchId = @"CREDIT-CARD-ITEMS";

constexpr int testSuggestionsMaxCount = 4;

class MockAutofillPopupController : public autofill::AutofillPopupController {
 public:
  MockAutofillPopupController()
      : default_font_desc_setter_("Arial, Times New Roman, 15px") {
    suggestions_.push_back(
        autofill::Suggestion("bufflehead", "canvasback", "goldeneye", 1));
    suggestions_.push_back(
        autofill::Suggestion("yellowlegs", "killdeer", "sandpiper", 1));
    suggestions_.push_back(
        autofill::Suggestion("phoebe", "flycatcher", "tyrant", 1));
    suggestions_.push_back(
        autofill::Suggestion("scrubjay", "bluejay", "stellersjay", 1));
  }

  // AutofillPopupViewDelegate
  MOCK_METHOD1(Hide, void(autofill::PopupHidingReason));
  MOCK_METHOD0(ViewDestroyed, void());
  MOCK_METHOD1(SetSelectionAtPoint, void(const gfx::Point& point));
  MOCK_METHOD0(AcceptSelectedLine, bool());
  MOCK_METHOD0(SelectionCleared, void());
  MOCK_CONST_METHOD0(HasSelection, bool());
  MOCK_CONST_METHOD0(popup_bounds, gfx::Rect());
  MOCK_CONST_METHOD0(container_view, gfx::NativeView());
  MOCK_CONST_METHOD0(GetWebContents, content::WebContents*());
  MOCK_CONST_METHOD0(element_bounds, const gfx::RectF&());
  MOCK_CONST_METHOD0(IsRTL, bool());
  std::vector<autofill::Suggestion> GetSuggestions() const override {
    return suggestions_;
  }

  // AutofillPopupController
  MOCK_METHOD0(OnSuggestionsChanged, void());
  MOCK_METHOD1(AcceptSuggestion, void(int index));

  int GetLineCount() const override { return line_count_; }

  const autofill::Suggestion& GetSuggestionAt(int row) const override {
    return suggestions_.at(row);
  }

  std::u16string GetSuggestionMainTextAt(int row) const override {
    return suggestions_.at(row).main_text.value;
  }

  std::u16string GetSuggestionMinorTextAt(int row) const override {
    return std::u16string();
  }

  std::vector<std::vector<autofill::Suggestion::Text>> GetSuggestionLabelsAt(
      int row) const override {
    return suggestions_[row].labels;
  }

  MOCK_METHOD3(GetRemovalConfirmationText,
               bool(int index, std::u16string* title, std::u16string* body));
  MOCK_METHOD1(RemoveSuggestion, bool(int index));
  MOCK_METHOD1(SetSelectedLine, void(absl::optional<int> selected_line));
  MOCK_CONST_METHOD0(selected_line, absl::optional<int>());
  MOCK_CONST_METHOD0(GetPopupType, autofill::PopupType());

  void set_line_count(int line_count) {
    EXPECT_LE(line_count, testSuggestionsMaxCount);
    line_count_ = line_count;
  }

 private:
  int line_count_;
  std::vector<autofill::Suggestion> suggestions_;
  gfx::ScopedDefaultFontDescription default_font_desc_setter_;
};

class CreditCardAutofillTouchBarControllerUnitTest : public CocoaTest {
 public:
  void SetUp() override {
    CocoaTest::SetUp();

    touch_bar_controller_.reset([[CreditCardAutofillTouchBarController alloc]
        initWithController:&autofill_popup_controller_]);
  }

  void SetLineCount(int line_count) {
    autofill_popup_controller_.set_line_count(line_count);
  }

  base::scoped_nsobject<CreditCardAutofillTouchBarController>
      touch_bar_controller_;
  MockAutofillPopupController autofill_popup_controller_;
};

// Tests to check if the touch bar shows up properly.
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, TouchBar) {
  // Touch bar shouldn't appear if the popup is not for credit cards.
  [touch_bar_controller_ setIsCreditCardPopup:false];
  EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

  // Touch bar shouldn't appear if the popup is empty.
  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetLineCount(0);
  EXPECT_FALSE([touch_bar_controller_ makeTouchBar]);

  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetLineCount(2);
  NSTouchBar* touch_bar = [touch_bar_controller_ makeTouchBar];
  EXPECT_TRUE(touch_bar);
  EXPECT_TRUE([[touch_bar customizationIdentifier]
      isEqual:ui::GetTouchBarId(kCreditCardAutofillTouchBarId)]);
  EXPECT_EQ(1UL, [[touch_bar itemIdentifiers] count]);
}

// Tests to check that the touch bar doesn't show more than 3 items
TEST_F(CreditCardAutofillTouchBarControllerUnitTest, TouchBarCardLimit) {
  [touch_bar_controller_ setIsCreditCardPopup:true];
  SetLineCount(4);
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
  SetLineCount(1);
  NSButton* button = [touch_bar_controller_ createCreditCardButtonAtRow:0];
  EXPECT_TRUE(button);
  EXPECT_EQ(0, [button tag]);
  EXPECT_EQ("bufflehead canvasback", base::SysNSStringToUTF8([button title]));
}

}  // namespace
