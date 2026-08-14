// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/suggestions/suggestion_util.h"

#include <algorithm>
#include <vector>

#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Returns true if `suggestion` is equal to `target` or if `target` is a
// descendant of `suggestion` at any depth in `suggestion.children`.
bool IsOrHasDescendant(const Suggestion& suggestion, const Suggestion& target) {
  return suggestion == target ||
         std::ranges::any_of(suggestion.children,
                             [&target](const Suggestion& child) {
                               return IsOrHasDescendant(child, target);
                             });
}

// Recursively deactivates `suggestion` and all of its descendants by setting
// their acceptability to `kUnselectableAndUnacceptable` and ensuring their
// loading state is false.
void DisableSuggestionAndDescendants(Suggestion& suggestion) {
  suggestion.acceptability =
      Suggestion::Acceptability::kUnselectableAndUnacceptable;
  suggestion.is_loading = Suggestion::IsLoading(false);
  for (Suggestion& child : suggestion.children) {
    DisableSuggestionAndDescendants(child);
  }
}

}  // namespace

AutocompleteUnrecognizedBehavior GetAcUnrecognizedBehavior(
    const AutofillClient& autofill_client) {
  return autofill_client.IsTabInActorMode()
             ? AutocompleteUnrecognizedBehavior::kSuggestionsAllowed
             : AutocompleteUnrecognizedBehavior::kSuggestionsSuppressed;
}

bool SuppressSuggestionsForAutocompleteUnrecognizedField(
    const AutofillField& field,
    AutocompleteUnrecognizedBehavior behavior) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return false;
#else
  return field.ShouldSuppressSuggestionsAndFillingByDefault(behavior);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

std::vector<Suggestion> PrepareLoadingStateSuggestions(
    std::vector<Suggestion> current_suggestions,
    const Suggestion& selected_suggestion) {
  for (Suggestion& suggestion : current_suggestions) {
    using enum Suggestion::Acceptability;
    if (IsOrHasDescendant(suggestion, selected_suggestion)) {
      suggestion.acceptability = kSelectableButUnacceptable;
      suggestion.is_loading = Suggestion::IsLoading(true);
    } else {
      suggestion.acceptability = kUnselectableAndUnacceptable;
      suggestion.is_loading = Suggestion::IsLoading(false);
    }
    for (Suggestion& child : suggestion.children) {
      DisableSuggestionAndDescendants(child);
    }
  }
  return current_suggestions;
}

Suggestion CreateUndoSuggestion() {
  Suggestion suggestion(l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM),
                        SuggestionType::kUndo);
  suggestion.icon = Suggestion::Icon::kUndo;
  // TODO(crbug.com/40266549): update "Clear Form" a11y announcement to "Undo"
  suggestion.acceptance_a11y_announcement =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  return suggestion;
}

bool IsManagementFooterOption(const Suggestion& suggestion) {
  switch (suggestion.type) {
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageAutofillAi:
    case SuggestionType::kManageAutofillAiIdentityDocs:
    case SuggestionType::kManageAutofillAiShopping:
    case SuggestionType::kManageAutofillAiTravel:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManageLoyaltyCard:
    case SuggestionType::kManageEnhancedAutofill:
    case SuggestionType::kOpenGemini:
    case SuggestionType::kWebauthnPasskeyQrCode:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAddressEntry:
    case SuggestionType::kAddressEntryOnTyping:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kAllLoyaltyCardsEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAtMemoryAiDisclosure:
    case SuggestionType::kAtMemoryFetching:
    case SuggestionType::kAtMemoryGenericError:
    case SuggestionType::kAtMemoryInactivityNudge:
    case SuggestionType::kAtMemoryNoConnection:
    case SuggestionType::kAtMemorySearchAffordance:
    case SuggestionType::kAtMemorySearchResult:
    case SuggestionType::kAtMemorySourceAttribution:
    case SuggestionType::kAutocompleteAtMemoryButton:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kAutofillAiOtherOrders:
    case SuggestionType::kAutofillAiOtherShipments:
    case SuggestionType::kAutofillAiPrivateInferenceNotice:
    case SuggestionType::kBackupPasswordEntry:
    case SuggestionType::kBnplEntry:
    case SuggestionType::kBnplFootnote:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kFetchingAmbientData:
    case SuggestionType::kFillAutofillAi:
    case SuggestionType::kFillPassword:
    case SuggestionType::kFreeformFooter:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kIdentityCredential:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kLoadingThrobber:
    case SuggestionType::kLoyaltyCardEntry:
    case SuggestionType::kMaximizeCreditCardBenefitsEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kOneTimePasswordEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kPendingStateSignin:
    case SuggestionType::kPersonalContextNotice:
    case SuggestionType::kRemoveAutofillAi:
    case SuggestionType::kSaveAndFillCreditCardEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kSeparator:
    case SuggestionType::kTitle:
    case SuggestionType::kTroubleSigningInEntry:
    case SuggestionType::kUndo:
    case SuggestionType::kViewPasswordDetails:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kWebauthnCredential:
      return false;
  }
}

}  // namespace autofill
