// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion_conversion.h"

#import <UIKit/UIKit.h>

#import <variant>

#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/data_model/payments/credit_card.h"
#import "components/autofill/core/browser/filling/filling_product.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/gfx/image/image.h"

using base::SysUTF16ToNSString;

namespace autofill {

namespace {

// The correct icon size to use in suggestions. Used to ensure images are scaled
// appropriately.
constexpr CGFloat kSuggestionIconWidth = 32;

// Gets the icon that will be used for the specified suggestion.
SuggestionIconType GetSuggestionIconType(const Suggestion& suggestion,
                                         BOOL hasValue) {
  if (suggestion.icon == Suggestion::Icon::kUndo) {
    return SuggestionIconType::kUndoAutofill;
  } else if (suggestion.icon == Suggestion::Icon::kHome && hasValue) {
    return SuggestionIconType::kAccountHome;
  } else if (suggestion.icon == Suggestion::Icon::kWork && hasValue) {
    return SuggestionIconType::kAccountWork;
  }
  return SuggestionIconType::kNone;
}

// Creates the icon for a payment card suggestion.
UIImage* CreateCreditCardIcon(const Suggestion& popup_suggestion) {
  // If available, the custom icon for the card is preferred over the
  // generic network icon. The network icon may also be missing, in
  // which case we do not set an icon at all.
  if (auto* custom_icon =
          std::get_if<gfx::Image>(&popup_suggestion.custom_icon);
      custom_icon && !custom_icon->IsEmpty()) {
    UIImage* icon = custom_icon->ToUIImage();

    // On iOS, the keyboard accessory wants smaller icons than the default
    // 40x24 size, so we resize them to 32x20, if the provided icon is
    // larger than that.
    if (icon && (icon.size.width > kSuggestionIconWidth)) {
      // For a simple image resize, we can keep the same underlying image
      // and only adjust the ratio.
      CGFloat ratio = icon.size.width / kSuggestionIconWidth;
      return [UIImage imageWithCGImage:[icon CGImage]
                                 scale:icon.scale * ratio
                           orientation:icon.imageOrientation];
    }
    return icon;
  } else if (popup_suggestion.icon != Suggestion::Icon::kNoIcon) {
    const int resourceID = CreditCard::IconResourceId(popup_suggestion.icon);
    return ui::ResourceBundle::GetSharedInstance()
        .GetNativeImageNamed(resourceID)
        .ToUIImage();
  }
  return nil;
}

}  // namespace

FormSuggestion* FormSuggestionFromAutofillSuggestion(
    const Suggestion& popup_suggestion,
    size_t index,
    const base::WeakPtr<AutofillSuggestionDelegate>& delegate,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  // Only fillable or actionable types (e.g. address, credit card, autocomplete,
  // undo) are converted; non-fillable items like headers or separators are
  // omitted.
  NSString* value = nil;
  NSString* minorValue = nil;
  NSString* displayDescription = nil;
  UIImage* icon = nil;
  switch (popup_suggestion.type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
      if ((!base::FeatureList::IsEnabled(
               features::kAutofillEnableBottomSheetScanCardAndFill) &&
           popup_suggestion.type ==
               SuggestionType::kSaveAndFillCreditCardEntry)) {
        break;
      }

      // `value` will contain the text to be filled in the selected element
      // while `displayDescription` will contain a summary of the data to be
      // filled in the other elements.
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
      if (!popup_suggestion.labels.empty() &&
          !popup_suggestion.labels.front().empty()) {
        DCHECK_EQ(popup_suggestion.labels.size(), 1U);
        DCHECK_EQ(popup_suggestion.labels[0].size(), 1U);
        displayDescription =
            SysUTF16ToNSString(popup_suggestion.labels[0][0].value);
      }

      if (!popup_suggestion.minor_texts.empty()) {
        // For Virtual Cards, the `main_text` is just "Virtual card" so we
        // need to include the `minor_text` (which is the card name + last 4
        // digits || card holder's name) as the `minorValue`.
        minorValue = SysUTF16ToNSString(popup_suggestion.minor_texts[0].value);
      }

      // Only show icon for credit card suggestions.
      if (delegate &&
          delegate->GetMainFillingProduct() == FillingProduct::kCreditCard) {
        icon = CreateCreditCardIcon(popup_suggestion);
      }
      break;

    case SuggestionType::kUndo:
      // There's no information to set, but this will not be discarded because
      // `suggestionIconType` will be set below.
      break;

    case SuggestionType::kFetchingAmbientData:
      value = SysUTF16ToNSString(popup_suggestion.main_text.value);
      break;

    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryAiDisclosure:
    case SuggestionType::kAtMemoryFetching:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemoryOpenGemini:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAtMemoryShortcutSettingsPromo:
    case SuggestionType::kAtMemorySourceAttribution:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiOtherShipments:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kAutofillAiSourceAttribution:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kGmailOneTimePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManageOffers:
    case SuggestionType::kManageEnhancedAutofill:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kOpenGmailForOtps:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kPendingStateTrustedVaultError:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kRemoveAutofillAi:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      break;
  }

  SuggestionIconType suggestionIconType =
      GetSuggestionIconType(popup_suggestion, value.length > 0);

  // Skip if there is no text or icon to show.
  if (!value && !icon && suggestionIconType == SuggestionIconType::kNone) {
    return nil;
  }

  NSString* acceptanceA11yAnnouncement =
      popup_suggestion.acceptance_a11y_announcement.has_value()
          ? SysUTF16ToNSString(*popup_suggestion.acceptance_a11y_announcement)
          : nil;

  FieldType fieldByFieldFillingTypeUsed =
      (popup_suggestion.field_by_field_filling_type_used
           ? *popup_suggestion.field_by_field_filling_type_used
           : FieldType::EMPTY_TYPE);

  FormSuggestionMetadata metadata;
  metadata.suggestion_delegate = delegate;
  metadata.form_id = form_id;
  metadata.field_id = field_id;
  metadata.popup_suggestion_index = index;

  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:value
                               minorValue:minorValue
                       displayDescription:displayDescription
                                     icon:icon
                                     type:popup_suggestion.type
                                  payload:popup_suggestion.payload
              fieldByFieldFillingTypeUsed:fieldByFieldFillingTypeUsed
                           requiresReauth:NO
               acceptanceA11yAnnouncement:acceptanceA11yAnnouncement
                                 metadata:metadata];

  suggestion.featureForIPH = SuggestionFeatureForIPH::kUnknown;
  suggestion.suggestionIconType = suggestionIconType;
  if (popup_suggestion.iph_metadata.feature ==
      &feature_engagement::
          kIPHAutofillExternalAccountProfileSuggestionFeature) {
    suggestion.featureForIPH =
        SuggestionFeatureForIPH::kAutofillExternalAccountProfile;
  } else if (popup_suggestion.iph_metadata.feature ==
             &feature_engagement::
                 kIPHAutofillHomeWorkProfileSuggestionFeature) {
    suggestion.featureForIPH =
        SuggestionFeatureForIPH::kHomeAndWorkAddressSuggestion;
  } else if (popup_suggestion.iph_metadata.feature ==
             &feature_engagement::
                 kIPHAutofillAccountNameEmailSuggestionFeature) {
    suggestion.featureForIPH =
        SuggestionFeatureForIPH::kAccountNameEmailSuggestion;
  } else if (popup_suggestion.iph_metadata.feature ==
             &feature_engagement::
                 kIPH_AutofillAmbientAutofillSpiiDisclaimer) {
    suggestion.featureForIPH =
        SuggestionFeatureForIPH::kAutofillAmbientAutofillSpiiDisclaimer;
  }
  return suggestion;
}

NSArray<FormSuggestion*>* FormSuggestionsFromAutofillSuggestions(
    const std::vector<Suggestion>& suggestions,
    const base::WeakPtr<AutofillSuggestionDelegate>& delegate,
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  NSMutableArray<FormSuggestion*>* form_suggestions =
      [[NSMutableArray alloc] init];
  for (size_t i = 0; i < suggestions.size(); ++i) {
    FormSuggestion* form_suggestion = FormSuggestionFromAutofillSuggestion(
        suggestions[i], i, delegate, form_id, field_id);
    if (!form_suggestion) {
      continue;
    }
    // Put the Undo suggestion at the front of the suggestions.
    if (suggestions[i].type == SuggestionType::kUndo) {
      [form_suggestions insertObject:form_suggestion atIndex:0];
    } else {
      [form_suggestions addObject:form_suggestion];
    }
  }
  return form_suggestions;
}

}  // namespace autofill
