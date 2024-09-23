// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/touchbar/credit_card_autofill_touch_bar_controller.h"

#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/app/vector_icons/vector_icons.h"
#import "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/ui/autofill_resource_utils.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/grit/components_scaled_resources.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia_util_mac.h"
#include "ui/gfx/paint_vector_icon.h"

namespace {

// Touch bar identifier.
NSString* const kCreditCardAutofillTouchBarId = @"credit-card-autofill";

// Touch bar items identifiers.
NSString* const kCreditCardTouchId = @"CREDIT-CARD";
NSString* const kCreditCardItemsTouchId = @"CREDIT-CARD-ITEMS";

// Maximum number of autofill items that can appear on the touch bar.
constexpr int maxTouchBarItems = 3;

// Returns the credit card image.
NSImage* GetCreditCardTouchBarImage(int iconId) {
  if (iconId < 1) {
    return nil;
  }

  // If it's a generic card image, use the vector icon instead.
  if (iconId == IDR_AUTOFILL_CC_GENERIC) {
    return NSImageFromImageSkia(
        gfx::CreateVectorIcon(kCreditCardIcon, 16, SK_ColorWHITE));
  }

  return ui::ResourceBundle::GetSharedInstance()
      .GetNativeImageNamed(iconId)
      .AsNSImage();
}

}  // namespace

@interface CreditCardAutofillTouchBarController ()
- (NSColor*)touchBarSubtextColor;
@end

@implementation CreditCardAutofillTouchBarController {
  raw_ptr<autofill::AutofillPopupController> _controller;  // weak
  bool _is_credit_card_popup;
}

- (instancetype)initWithController:
    (autofill::AutofillPopupController*)controller {
  if ((self = [super init])) {
    _controller = controller;
    _is_credit_card_popup = (_controller->GetMainFillingProduct() ==
                             autofill::FillingProduct::kCreditCard);
  }
  return self;
}

- (NSTouchBar*)makeTouchBar {
  if (!_controller->GetLineCount() || !_is_credit_card_popup) {
    return nil;
  }

  NSTouchBar* touchBar = [[NSTouchBar alloc] init];
  [touchBar setCustomizationIdentifier:ui::GetTouchBarId(
                                           kCreditCardAutofillTouchBarId)];
  touchBar.delegate = self;

  [touchBar setDefaultItemIdentifiers:@[ kCreditCardItemsTouchId ]];
  return touchBar;
}

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier {
  if (![identifier hasSuffix:kCreditCardItemsTouchId]) {
    return nil;
  }

  NSMutableArray* creditCardItems = [NSMutableArray array];
  for (int i = 0; i < _controller->GetLineCount() && i < maxTouchBarItems;
       i++) {
    const autofill::Suggestion& suggestion = _controller->GetSuggestionAt(i);
    if (suggestion.type != autofill::SuggestionType::kAutocompleteEntry &&
        suggestion.type != autofill::SuggestionType::kAddressEntry &&
        suggestion.type != autofill::SuggestionType::kCreditCardEntry) {
      continue;
    }

    NSString* cardIdentifier = [NSString
        stringWithFormat:@"%@-%i",
                         ui::GetTouchBarItemId(kCreditCardAutofillTouchBarId,
                                               kCreditCardTouchId),
                         i];
    NSCustomTouchBarItem* item =
        [[NSCustomTouchBarItem alloc] initWithIdentifier:cardIdentifier];
    [item setView:[self createCreditCardButtonAtRow:i]];
    [creditCardItems addObject:item];
  }

  return [NSGroupTouchBarItem groupItemWithIdentifier:identifier
                                                items:creditCardItems];
}

- (NSColor*)touchBarSubtextColor {
  return [NSColor colorWithCalibratedRed:206.0 / 255.0
                                   green:206.0 / 255.0
                                    blue:206.0 / 255.0
                                   alpha:1.0];
}

- (NSButton*)createCreditCardButtonAtRow:(int)row {
  const autofill::Suggestion& suggestion = _controller->GetSuggestionAt(row);
  NSString* label = base::SysUTF16ToNSString(suggestion.main_text.value);
  NSString* subtext = nil;
  std::vector<std::vector<autofill::Suggestion::Text>> suggestion_labels =
      suggestion.labels;
  if (!suggestion_labels.empty()) {
    CHECK_GT(suggestion_labels.size(), 0U);
    CHECK_GT(suggestion_labels[0].size(), 0U);
    subtext =
        base::SysUTF16ToNSString(std::move(suggestion_labels[0][0].value));
  }

  // Create the button title based on the text direction.
  NSString* buttonTitle =
      [subtext length] ? [NSString stringWithFormat:@"%@ %@", label, subtext]
                       : label;

  // Create the button.
  NSImage* cardIconImage =
      GetCreditCardTouchBarImage(autofill::GetIconResourceID(suggestion.icon));
  NSButton* button = nil;
  if (cardIconImage) {
    button = [NSButton buttonWithTitle:buttonTitle
                                 image:cardIconImage
                                target:self
                                action:@selector(acceptCreditCard:)];
    button.imageHugsTitle = YES;
    button.imagePosition = base::i18n::IsRTL() ? NSImageLeft : NSImageRight;
  } else {
    button = [NSButton buttonWithTitle:buttonTitle
                                target:self
                                action:@selector(acceptCreditCard:)];
  }

  // Apply text attributes to the button so that the subtext will appear
  // smaller and lighter than the rest of the title.
  NSMutableAttributedString* attributedString =
      [button.attributedTitle mutableCopy];
  NSFont* subtextFont =
      [NSFontManager.sharedFontManager convertFont:button.font
                                            toSize:button.font.pointSize - 1];
  NSRange labelRange = NSMakeRange(0, label.length);
  NSRange subtextRange =
      NSMakeRange(buttonTitle.length - subtext.length, subtext.length);
  [attributedString addAttribute:NSForegroundColorAttributeName
                           value:NSColor.whiteColor
                           range:labelRange];
  [attributedString addAttribute:NSForegroundColorAttributeName
                           value:[self touchBarSubtextColor]
                           range:subtextRange];
  [attributedString addAttribute:NSFontAttributeName
                           value:subtextFont
                           range:subtextRange];
  button.attributedTitle = attributedString;

  // The tag is used to store the suggestion index.
  button.tag = row;

  return button;
}

- (void)acceptCreditCard:(id)sender {
  _controller->AcceptSuggestion([sender tag]);
}

- (void)setIsCreditCardPopup:(bool)is_credit_card_popup {
  _is_credit_card_popup = is_credit_card_popup;
}

@end
