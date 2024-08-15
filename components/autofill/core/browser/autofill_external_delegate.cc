// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/field_filling_address_util.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

namespace {

const AutofillProfile* GetTestAddressByGUID(
    base::span<const AutofillProfile> test_addresses,
    const std::string& guid) {
  if (test_addresses.empty()) {
    return nullptr;
  }
  auto it = base::ranges::find(test_addresses, guid, &AutofillProfile::guid);
  return it == test_addresses.end() ? nullptr : &(*it);
}

// Returns true if the suggestion entry is an Autofill warning message.
// Warning messages should display on top of suggestion list.
bool IsAutofillWarningEntry(SuggestionType type) {
  return type == SuggestionType::kInsecureContextPaymentDisabledMessage ||
         type == SuggestionType::kMixedFormMessage;
}

// The `AutofillTriggerSource` indicates what caused an Autofill fill or preview
// to happen. This can happen by selecting a suggestion, but also through a
// dynamic change (refills) or through a surface that doesn't use suggestions,
// like TTF. This function is concerned with the first case: A suggestion that
// was generated through the `suggestion_trigger_source` got selected. This
// function returns the appropriate `AutofillTriggerSource`.
// Note that an `AutofillSuggestionTriggerSource` is different from a
// `AutofillTriggerSource`. The former describes what caused the suggestion
// itself to appear. For example, depending on the completeness of the form,
// clicking into a field (the suggestion trigger source) can cause
// the keyboard accessory or TTF/fast checkout to appear (the trigger source).
AutofillTriggerSource TriggerSourceFromSuggestionTriggerSource(
    AutofillSuggestionTriggerSource suggestion_trigger_source) {
  switch (suggestion_trigger_source) {
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::
        kShowPromptAfterDialogClosedNonManualFallback:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
      // On Android, no popup exists. Instead, the keyboard accessory is used.
#if BUILDFLAG(IS_ANDROID)
      return AutofillTriggerSource::kKeyboardAccessory;
#else
      return AutofillTriggerSource::kPopup;
#endif  // BUILDFLAG(IS_ANDROID)
    case AutofillSuggestionTriggerSource::kManualFallbackAddress:
    case AutofillSuggestionTriggerSource::kManualFallbackPayments:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses:
      // Manual fallbacks are both a suggestion trigger source (e.g. through the
      // context menu) and a trigger source (by selecting a suggestion generated
      // through the context menu).
      return AutofillTriggerSource::kManualFallback;
    case AutofillSuggestionTriggerSource::kPredictionImprovements:
      return AutofillTriggerSource::kPredictionImprovements;
  }
  NOTREACHED();
}

// Returns a pointer to the first Suggestion whose GUID matches that of a
// AutofillClient::GetTestAddresses() profile.
const Suggestion* FindTestSuggestion(AutofillClient& client,
                                     base::span<const Suggestion> suggestions,
                                     int index) {
  auto is_test_suggestion = [&client](const Suggestion& suggestion) {
    auto* backend_id = absl::get_if<Suggestion::BackendId>(&suggestion.payload);
    auto* guid =
        backend_id ? absl::get_if<Suggestion::Guid>(backend_id) : nullptr;
    base::span<const AutofillProfile> test_addresses =
        client.GetTestAddresses();

    return guid && base::Contains(test_addresses, guid->value(),
                                  &AutofillProfile::guid);
  };
  for (const Suggestion& suggestion : suggestions) {
    if (is_test_suggestion(suggestion) && index-- == 0) {
      return &suggestion;
    }
  }
  return nullptr;
}

}  // namespace

int AutofillExternalDelegate::shortcut_test_suggestion_index_ = -1;

AutofillExternalDelegate::AutofillExternalDelegate(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

AutofillExternalDelegate::~AutofillExternalDelegate() = default;

// static
bool AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId(
    SuggestionType item_id) {
  switch (item_id) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kDevtoolsTestAddresses:
      // Virtual cards can appear on their own when filling the CVC for a card
      // that a merchant has saved. This indicates there could be Autofill
      // suggestions related to standalone CVC fields.
    case SuggestionType::kVirtualCreditCardEntry:
      return true;
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kAutocompleteEntry:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
      return false;
  }
}

void AutofillExternalDelegate::OnQuery(
    const FormData& form,
    const FormFieldData& field,
    const gfx::Rect& caret_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  query_form_ = form;
  query_field_ = field;
  caret_bounds_ = caret_bounds;
  trigger_source_ = trigger_source;
}

const AutofillField* AutofillExternalDelegate::GetQueriedAutofillField() const {
  return manager_->GetAutofillField(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions,
    std::optional<autofill_metrics::SuggestionRankingContext>
        suggestion_ranking_context) {
  if (field_id != query_field_.global_id()) {
    return;
  }
  suggestion_ranking_context_ = std::move(suggestion_ranking_context);

#if BUILDFLAG(IS_IOS)
  if (!manager_->client().IsLastQueriedField(field_id)) {
    return;
  }
#endif

  std::vector<Suggestion> suggestions(input_suggestions);

  // Hide warnings as appropriate.
  PossiblyRemoveAutofillWarnings(&suggestions);

  // TODO(crbug.com/320126773): consider moving these metrics to a better place.
  if (base::ranges::any_of(suggestions, [](const Suggestion& suggestion) {
        return suggestion.type == SuggestionType::kShowAccountCards;
      })) {
    autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
        autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
            kButtonAppeared);
    if (!show_cards_from_account_suggestion_was_shown_) {
      show_cards_from_account_suggestion_was_shown_ = true;
      autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
          autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
              kButtonAppearedOnce);
    }
  }

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&suggestions);

  if (suggestions.empty()) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
    // No suggestions, any popup currently showing is obsolete.
    manager_->client().HideAutofillSuggestions(
        SuggestionHidingReason::kNoSuggestions);
    return;
  }

  shown_suggestion_types_.clear();
  for (const Suggestion& suggestion : input_suggestions) {
    shown_suggestion_types_.push_back(suggestion.type);
  }

  // Send to display.
  if (query_field_.is_focusable() && manager_->driver().CanShowAutofillUi()) {
    if (shortcut_test_suggestion_index_ >= 0) {
      const Suggestion* test_suggestion = FindTestSuggestion(
          manager_->client(), suggestions, shortcut_test_suggestion_index_);
      CHECK(test_suggestion) << "Only test suggestions can shortcut the UI";
      DidAcceptSuggestion(*test_suggestion, {});
      return;
    }

    AutofillComposeDelegate* delegate = manager_->client().GetComposeDelegate();
    const bool show_proactive_nudge_at_caret =
        shown_suggestion_types_.size() == 1 &&
        shown_suggestion_types_[0] == SuggestionType::kComposeProactiveNudge &&
        (delegate && delegate->ShouldAnchorNudgeOnCaret());
    const bool are_caret_bounds_valid =
        caret_bounds_ != gfx::Rect() &&
        query_field_.bounds().Contains(gfx::RectF(caret_bounds_));
    const bool should_use_caret_bounds =
        show_proactive_nudge_at_caret && are_caret_bounds_valid;

    const PopupAnchorType default_anchor_type =
#if BUILDFLAG(IS_ANDROID)
        PopupAnchorType::kKeyboardAccessory;
#else
        PopupAnchorType::kField;
#endif
    AutofillClient::PopupOpenArgs open_args(
        should_use_caret_bounds ? gfx::RectF(caret_bounds_)
                                : query_field_.bounds(),
        query_field_.text_direction(), suggestions, trigger_source_,
        query_field_.form_control_ax_id(),
        should_use_caret_bounds ? PopupAnchorType::kCaret
                                : default_anchor_type);
    manager_->client().ShowAutofillSuggestions(open_args, GetWeakPtr());
  }
}

SuggestionType
AutofillExternalDelegate::GetLastAcceptedSuggestionToFillForSection(
    const Section& section) const {
  if (auto it = last_accepted_address_suggestion_for_address_form_section_.find(
          section);
      it != last_accepted_address_suggestion_for_address_form_section_.end()) {
    return it->second;
  }
  // In case no suggestions were accepted for this section, default to full form
  // filling suggestions.
  return SuggestionType::kAddressEntry;
}

bool AutofillExternalDelegate::HasActiveScreenReader() const {
#if BUILDFLAG(IS_IOS)
  // ui::AXPlatform is not supported on iOS. The rendering engine handles
  // a11y internally.
  return false;
#else
  // Note: This always returns false if ChromeVox is in use because the
  // process-wide AXMode is not updated in that case; except for Lacros, where
  // kScreenReader mirrors the spoken feedback preference.
  return ui::AXPlatform::GetInstance().GetMode().has_mode(
      ui::AXMode::kScreenReader);
#endif
}

void AutofillExternalDelegate::OnAutofillAvailabilityEvent(
    mojom::AutofillSuggestionAvailability suggestion_availability) {
  // Availability of suggestions should be communicated to Blink because
  // accessibility objects live in both the renderer and browser processes.
  manager_->driver().RendererShouldSetSuggestionAvailability(
      query_field_.global_id(), suggestion_availability);
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    std::vector<SelectOption> datalist) {
  datalist_ = std::move(datalist);
  manager_->client().UpdateAutofillDataListValues(datalist_);
}

absl::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
AutofillExternalDelegate::GetDriver() {
  return &manager_->driver();
}

void AutofillExternalDelegate::OnSuggestionsShown() {
  // Popups are expected to be Autofill or Autocomplete.
  DCHECK_NE(GetMainFillingProduct(), FillingProduct::kPassword);

  const bool has_autofill_suggestions = base::ranges::any_of(
      shown_suggestion_types_, IsAutofillAndFirstLayerSuggestionId);

  // If the popup was manually triggered on an unclassified field, the chances
  // are high that it has no regular suggestions, as it is the main usecase for
  // the manual fallback functionality. It is considered an acceptable
  // approximation, but it obviously doesn't cover many other cases, such as
  // manual triggering payment suggestions on an address field, and should
  // be reconsidered if some more critical cases are found.
  const bool likely_has_no_regular_autofilling_options =
      TriggerSourceFromSuggestionTriggerSource(trigger_source_) ==
          AutofillTriggerSource::kManualFallback &&
      (!GetQueriedAutofillField() ||
       GetQueriedAutofillField()->Type().IsUnknown());

  if (likely_has_no_regular_autofilling_options) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kNoSuggestions);
  } else if (has_autofill_suggestions) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutofillAvailable);
    if (base::Contains(shown_suggestion_types_,
                       SuggestionType::kDevtoolsTestAddresses)) {
      autofill_metrics::OnDevtoolsTestAddressesShown();
    }
  } else {
    // We send autocomplete availability event even though there might be no
    // autocomplete suggestions shown.
    // TODO(crbug.com/315748930): Provide AX event only for autocomplete
    // entries.
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutocompleteAvailable);
    if (base::Contains(shown_suggestion_types_,
                       SuggestionType::kAutocompleteEntry)) {
      AutofillMetrics::OnAutocompleteSuggestionsShown();
    }
  }

  manager_->DidShowSuggestions(shown_suggestion_types_, query_form_,
                               query_field_);

  if (base::Contains(shown_suggestion_types_,
                     SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
  }
}

void AutofillExternalDelegate::OnSuggestionsHidden() {
  manager_->OnSuggestionsHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const Suggestion& suggestion) {
  ClearPreviewedForm();

  const Suggestion::BackendId backend_id =
      suggestion.GetPayload<Suggestion::BackendId>();

  switch (suggestion.type) {
    case SuggestionType::kUndoOrClear:
#if !BUILDFLAG(IS_IOS)
      manager_->UndoAutofill(mojom::ActionPersistence::kPreview, query_form_,
                             query_field_);
#endif
      break;
    case SuggestionType::kAddressEntry:
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kDevtoolsTestAddressEntry:
      FillAutofillFormData(
          suggestion.type, backend_id, /*position=*/std::nullopt,
          /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kFillFullAddress:
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          /*position=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetAddressFieldsForGroupFilling()});
      break;
    case SuggestionType::kFillFullName:
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          /*position=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kName)});
      break;
    case SuggestionType::kFillFullPhoneNumber:
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          /*position=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kPhone)});
      break;
    case SuggestionType::kFillFullEmail:
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          /*position=*/std::nullopt, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kEmail)});
      break;
    case SuggestionType::kAutocompleteEntry:
      manager_->FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                   mojom::FieldActionType::kReplaceAll,
                                   query_form_, query_field_,
                                   suggestion.main_text.value, suggestion.type,
                                   /*field_type_used=*/std::nullopt);
      break;
    case SuggestionType::kIbanEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, IBAN_VALUE);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, MERCHANT_PROMO_CODE);
      break;
    case SuggestionType::kFillExistingPlusAddress:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::FieldActionType::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.type, EMAIL_ADDRESS);
      break;
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
      PreviewFieldByFieldFillingSuggestion(suggestion);
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      // If triggered on a non payments form, don't preview the value.
      if (IsPaymentsManualFallbackOnNonPaymentsField()) {
        break;
      }
      FillAutofillFormData(
          suggestion.type, backend_id, /*position=*/std::nullopt,
          /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kDevtoolsTestAddresses:
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
      NOTREACHED();  // Should be handled elsewhere.
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  CHECK(suggestion.is_acceptable);
  base::UmaHistogramEnumeration("Autofill.Suggestions.AcceptedType",
                                suggestion.type);

  switch (suggestion.type) {
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kDevtoolsTestAddressEntry:
      DidAcceptAddressSuggestion(suggestion, position);
      break;
    case SuggestionType::kCreditCardEntry:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kScanCreditCard:
      DidAcceptPaymentsSuggestion(suggestion, position);
      break;
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress: {
      manager_->client().ShowAutofillSettings(suggestion.type);
      break;
    }
    case SuggestionType::kUndoOrClear:
#if !BUILDFLAG(IS_IOS)
      manager_->UndoAutofill(mojom::ActionPersistence::kFill, query_form_,
                             query_field_);
#endif
      break;
    case SuggestionType::kDatalistEntry:
      manager_->driver().RendererShouldAcceptDataListSuggestion(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case SuggestionType::kAutocompleteEntry:
      AutofillMetrics::LogAutocompleteEvent(
          AutofillMetrics::AutocompleteEvent::AUTOCOMPLETE_SUGGESTION_SELECTED);
      autofill_metrics::LogSuggestionAcceptedIndex(
          position.row, FillingProduct::kAutocomplete,
          manager_->client().IsOffTheRecord());
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          suggestion.type, /*field_type_used=*/std::nullopt);
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kFillExistingPlusAddress:
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        plus_address_delegate->RecordAutofillSuggestionEvent(
            AutofillPlusAddressDelegate::SuggestionEvent::
                kExistingPlusAddressChosen);
      }
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          SuggestionType::kFillExistingPlusAddress, EMAIL_ADDRESS);
      break;
    case SuggestionType::kCreateNewPlusAddress: {
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        plus_address_delegate->RecordAutofillSuggestionEvent(
            AutofillPlusAddressDelegate::SuggestionEvent::
                kCreateNewPlusAddressChosen);
      }
      PlusAddressCallback callback = base::BindOnce(
          [](base::WeakPtr<AutofillExternalDelegate> delegate,
             const FormData& form, const FormFieldData& field,
             const std::string& plus_address) {
            if (delegate) {
              delegate->manager_->FillOrPreviewField(
                  mojom::ActionPersistence::kFill,
                  mojom::FieldActionType::kReplaceAll, form, field,
                  base::UTF8ToUTF16(plus_address),
                  SuggestionType::kCreateNewPlusAddress, EMAIL_ADDRESS);
            }
          },
          GetWeakPtr(), query_form_, query_field_);
      manager_->client().OfferPlusAddressCreation(
          manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
          std::move(callback));
      break;
    }
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->OpenCompose(
            manager_->driver(), query_field_.renderer_form_id(),
            query_field_.global_id(),
            autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);
      }
      break;
    case SuggestionType::kComposeDisable:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->DisableCompose();
      }
      break;
    case SuggestionType::kComposeGoToSettings:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->GoToSettings();
      }
      break;
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->NeverShowComposeForOrigin(
            manager_->client().GetLastCommittedPrimaryMainFrameOrigin());
      }
      break;
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kMixedFormMessage:
      // If the selected element is a warning we don't want to do anything.
      break;
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
      NOTREACHED();  // Should be handled elsewhere.
  }
  if (suggestion.type == SuggestionType::kShowAccountCards) {
    manager_->RefetchCardsAndUpdatePopup(query_form_, query_field_);
  } else {
    manager_->client().HideAutofillSuggestions(
        SuggestionHidingReason::kAcceptSuggestion);
  }
}

void AutofillExternalDelegate::DidPerformButtonActionForSuggestion(
    const Suggestion& suggestion) {
  switch (suggestion.type) {
    case SuggestionType::kComposeResumeNudge:
      NOTIMPLEMENTED();
      return;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

bool AutofillExternalDelegate::RemoveSuggestion(const Suggestion& suggestion) {
  switch (suggestion.type) {
    // These SuggestionTypes are various types which can appear in the first
    // level suggestion to fill an address or credit card field.
    case SuggestionType::kAddressEntry:
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kFillFullPhoneNumber:
    case SuggestionType::kAddressFieldByFieldFilling:
    case SuggestionType::kCreditCardFieldByFieldFilling:
    case SuggestionType::kCreditCardEntry:
      return manager_->RemoveAutofillProfileOrCreditCard(
          suggestion.GetPayload<Suggestion::BackendId>());
    case SuggestionType::kAutocompleteEntry:
      manager_->RemoveCurrentSingleFieldSuggestion(
          query_field_.name(), suggestion.main_text.value, suggestion.type);
      return true;
    case SuggestionType::kFillEverythingFromAddressProfile:
    case SuggestionType::kEditAddressProfile:
    case SuggestionType::kDeleteAddressProfile:
    case SuggestionType::kManageAddress:
    case SuggestionType::kManageCreditCard:
    case SuggestionType::kManageIban:
    case SuggestionType::kManagePlusAddress:
    case SuggestionType::kCreateNewPlusAddress:
    case SuggestionType::kFillExistingPlusAddress:
    case SuggestionType::kInsecureContextPaymentDisabledMessage:
    case SuggestionType::kScanCreditCard:
    case SuggestionType::kVirtualCreditCardEntry:
    case SuggestionType::kIbanEntry:
    case SuggestionType::kPasswordEntry:
    case SuggestionType::kAllSavedPasswordsEntry:
    case SuggestionType::kGeneratePasswordEntry:
    case SuggestionType::kShowAccountCards:
    case SuggestionType::kPasswordAccountStorageOptIn:
    case SuggestionType::kPasswordAccountStorageOptInAndGenerate:
    case SuggestionType::kAccountStoragePasswordEntry:
    case SuggestionType::kPasswordAccountStorageReSignin:
    case SuggestionType::kPasswordAccountStorageEmpty:
    case SuggestionType::kComposeResumeNudge:
    case SuggestionType::kComposeDisable:
    case SuggestionType::kComposeGoToSettings:
    case SuggestionType::kComposeNeverShowOnThisSiteAgain:
    case SuggestionType::kComposeProactiveNudge:
    case SuggestionType::kComposeSavedStateNotification:
    case SuggestionType::kDatalistEntry:
    case SuggestionType::kMerchantPromoCodeEntry:
    case SuggestionType::kSeePromoCodeDetails:
    case SuggestionType::kWebauthnCredential:
    case SuggestionType::kWebauthnSignInWithAnotherDevice:
    case SuggestionType::kTitle:
    case SuggestionType::kSeparator:
    case SuggestionType::kUndoOrClear:
    case SuggestionType::kMixedFormMessage:
    case SuggestionType::kDevtoolsTestAddresses:
    case SuggestionType::kDevtoolsTestAddressEntry:
    case SuggestionType::kDevtoolsTestAddressByCountry:
    case SuggestionType::kPasswordFieldByFieldFilling:
    case SuggestionType::kFillPassword:
    case SuggestionType::kViewPasswordDetails:
      return false;
  }
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client().HideAutofillSuggestions(
      SuggestionHidingReason::kEndEditing);
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  manager_->driver().RendererShouldClearPreviewedForm();
}

FillingProduct AutofillExternalDelegate::GetMainFillingProduct() const {
  bool has_plus_address_suggestion = false;
  for (SuggestionType type : shown_suggestion_types_) {
    if (FillingProduct product = GetFillingProductFromSuggestionType(type);
        product != FillingProduct::kNone) {
      // Plus address is considered to be the main filling product of the popup
      // only if it is the only fillable type in the suggestions list.
      if (product != FillingProduct::kPlusAddresses) {
        return product;
      }
      has_plus_address_suggestion = true;
    }
  }
  return has_plus_address_suggestion ? FillingProduct::kPlusAddresses
                                     : FillingProduct::kNone;
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::ShowEditAddressProfileDialog(
    const std::string& guid) {
  const AutofillProfile* profile = manager_->client()
                                       .GetPersonalDataManager()
                                       ->address_data_manager()
                                       .GetProfileByGUID(guid);
  if (profile) {
    manager_->client().ShowEditAddressProfileDialog(
        *profile,
        base::BindOnce(&AutofillExternalDelegate::OnAddressEditorClosed,
                       GetWeakPtr()));
  }
}

void AutofillExternalDelegate::ShowDeleteAddressProfileDialog(
    const std::string& guid) {
  const AutofillProfile* profile = manager_->client()
                                       .GetPersonalDataManager()
                                       ->address_data_manager()
                                       .GetProfileByGUID(guid);
  if (profile) {
    manager_->client().ShowDeleteAddressProfileDialog(
        *profile,
        base::BindOnce(&AutofillExternalDelegate::OnDeleteDialogClosed,
                       GetWeakPtr(), guid));
  }
}

void AutofillExternalDelegate::OnAddressEditorClosed(
    AutofillClient::AddressPromptUserDecision decision,
    base::optional_ref<const AutofillProfile> edited_profile) {
  if (decision == AutofillClient::AddressPromptUserDecision::kEditAccepted) {
    autofill_metrics::LogEditAddressProfileDialogClosed(
        /*user_saved_changes=*/true);
    AddressDataManager& adm =
        manager_->client().GetPersonalDataManager()->address_data_manager();
    if (!adm_observation_.IsObserving()) {
      adm_observation_.Observe(&adm);
    }
    CHECK(edited_profile.has_value());
    adm.UpdateProfile(edited_profile.value());
    return;
  }
  autofill_metrics::LogEditAddressProfileDialogClosed(
      /*user_saved_changes=*/false);
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnDeleteDialogClosed(const std::string& guid,
                                                    bool user_accepted_delete) {
  autofill_metrics::LogDeleteAddressProfileFromExtendedMenu(
      user_accepted_delete);
  if (user_accepted_delete) {
    AddressDataManager& adm =
        manager_->client().GetPersonalDataManager()->address_data_manager();
    if (!adm_observation_.IsObserving()) {
      adm_observation_.Observe(&adm);
    }
    adm.RemoveProfile(guid);
    return;
  }
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnAddressDataChanged() {
  adm_observation_.Reset();
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, query_form_, query_field_, card,
      std::u16string(), {.trigger_source = trigger_source});
}

void AutofillExternalDelegate::PreviewFieldByFieldFillingSuggestion(
    const Suggestion& suggestion) {
  CHECK(suggestion.type == SuggestionType::kAddressFieldByFieldFilling ||
        suggestion.type == SuggestionType::kCreditCardFieldByFieldFilling);
  CHECK(suggestion.field_by_field_filling_type_used);
  const auto guid = suggestion.GetBackendId<Suggestion::Guid>().value();
  if (const AutofillProfile* profile = manager_->client()
                                           .GetPersonalDataManager()
                                           ->address_data_manager()
                                           .GetProfileByGUID(guid)) {
    PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
  } else if (manager_->client()
                 .GetPersonalDataManager()
                 ->payments_data_manager()
                 .GetCreditCardByGUID(guid)) {
    PreviewCreditCardFieldByFieldFillingSuggestion(suggestion);
  }
}

void AutofillExternalDelegate::FillFieldByFieldFillingSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  CHECK(suggestion.type == SuggestionType::kAddressFieldByFieldFilling ||
        suggestion.type == SuggestionType::kCreditCardFieldByFieldFilling);
  CHECK(suggestion.field_by_field_filling_type_used);
  const auto guid = suggestion.GetBackendId<Suggestion::Guid>().value();
  if (const AutofillProfile* profile = manager_->client()
                                           .GetPersonalDataManager()
                                           ->address_data_manager()
                                           .GetProfileByGUID(guid)) {
    FillAddressFieldByFieldFillingSuggestion(*profile, suggestion, position);
  } else if (const CreditCard* credit_card = manager_->client()
                                                 .GetPersonalDataManager()
                                                 ->payments_data_manager()
                                                 .GetCreditCardByGUID(guid)) {
    FillCreditCardFieldByFieldFillingSuggestion(*credit_card, suggestion);
  }
}

void AutofillExternalDelegate::PreviewAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion) {
  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      profile, manager_->app_locale(),
      AutofillType(*suggestion.field_by_field_filling_type_used), query_field_,
      manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.type,
        suggestion.field_by_field_filling_type_used);
  }
}

void AutofillExternalDelegate::FillAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
  if (autofill_trigger_field && position.sub_popup_level > 0) {
    // We only update this when the user accepts a subpopup suggestion since the
    // filling granularity doesn't change by accepting a top-level popup
    // suggestion but stays the same.
    last_accepted_address_suggestion_for_address_form_section_
        [autofill_trigger_field->section()] =
            SuggestionType::kAddressFieldByFieldFilling;
  }
  const bool is_triggering_field_address =
      autofill_trigger_field &&
      IsAddressType(autofill_trigger_field->Type().GetStorableType());

  autofill_metrics::LogFillingMethodUsed(
      FillingMethod::kFieldByFieldFilling, FillingProduct::kAddress,
      /*triggering_field_type_matches_filling_product=*/
      is_triggering_field_address);

  // Only log the field-by-field filling type used if it was accepted from
  // a suggestion in a subpopup. The root popup can have field-by-field
  // suggestions after a field-by-field suggestion was accepted from a
  // subpopup, this is done to keep the user in a certain filling
  // granularity during their filling experience. However only the
  // subpopups field-by-field-filling types are statically built, based on
  // what we think is useful/handy (this will in the future vary per
  // country, see crbug.com/1502162), while field-by-field filling
  // suggestions in the root popup are dynamically built depending on the
  // triggering field type, which means that selecting them is the only
  // option users have in the first level. Therefore we only emit logs for
  // subpopup acceptance to measure the efficiency of the types we chose
  // and potentially remove/add new ones.
  if (position.sub_popup_level > 0) {
    autofill_metrics::LogFieldByFieldFillingFieldUsed(
        *suggestion.field_by_field_filling_type_used, FillingProduct::kAddress,
        /*triggering_field_type_matches_filling_product=*/
        is_triggering_field_address);
  }

  const auto& [filling_value, filling_type] = GetFillingValueAndTypeForProfile(
      profile, manager_->app_locale(),
      AutofillType(*suggestion.field_by_field_filling_type_used), query_field_,
      manager_->client().GetAddressNormalizer());
  if (!filling_value.empty()) {
    manager_->FillOrPreviewField(
        mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.type,
        suggestion.field_by_field_filling_type_used);
    manager_->OnDidFillAddressFormFillingSuggestion(
        profile, query_form_, query_field_,
        TriggerSourceFromSuggestionTriggerSource(trigger_source_));
  }
}

void AutofillExternalDelegate::PreviewCreditCardFieldByFieldFillingSuggestion(
    const Suggestion& suggestion) {
  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kPreview, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_, suggestion.main_text.value, suggestion.type,
      suggestion.field_by_field_filling_type_used);
}

void AutofillExternalDelegate::FillCreditCardFieldByFieldFillingSuggestion(
    const CreditCard& credit_card,
    const Suggestion& suggestion) {
  if (*suggestion.field_by_field_filling_type_used == CREDIT_CARD_NUMBER) {
    manager_->GetCreditCardAccessManager().FetchCreditCard(
        &credit_card,
        base::BindOnce(&AutofillExternalDelegate::OnCreditCardFetched,
                       GetWeakPtr()));
    return;
  }
  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_, suggestion.main_text.value, suggestion.type,
      suggestion.field_by_field_filling_type_used);
}

void AutofillExternalDelegate::OnCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess) {
    return;
  }
  // In the failure case, `credit_card` can be `nullptr`, but in the success
  // case it is non-null.
  CHECK(credit_card);

  manager_->OnCreditCardFetchedSuccessfully(*credit_card);
  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_,
      credit_card->GetInfo(CREDIT_CARD_NUMBER, manager_->app_locale()),
      SuggestionType::kCreditCardFieldByFieldFilling, CREDIT_CARD_NUMBER);
}

void AutofillExternalDelegate::OnVirtualCreditCardFetched(
    CreditCardFetchResult result,
    const CreditCard* credit_card) {
  if (result != CreditCardFetchResult::kSuccess) {
    return;
  }
  // In the failure case, `credit_card` can be `nullptr`, but in the success
  // case it is non-null.
  CHECK(credit_card);
  manager_->OnCreditCardFetchedSuccessfully(*credit_card);
}

void AutofillExternalDelegate::FillAutofillFormData(
    SuggestionType type,
    Suggestion::BackendId backend_id,
    std::optional<SuggestionPosition> position,
    bool is_preview,
    const AutofillTriggerDetails& trigger_details) {
  CHECK(is_preview || position);
  // Only address suggestions store the last field types to fill. This is
  // because this is the only use case where filling granularies need to be
  // persisted.
  static constexpr auto kAutofillAddressSuggestions =
      base::MakeFixedFlatSet<SuggestionType>(
          {SuggestionType::kAddressEntry, SuggestionType::kFillFullAddress,
           SuggestionType::kFillFullPhoneNumber, SuggestionType::kFillFullEmail,
           SuggestionType::kFillFullName,
           SuggestionType::kFillEverythingFromAddressProfile});
  const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
  if (autofill_trigger_field && kAutofillAddressSuggestions.contains(type) &&
      !is_preview && position->sub_popup_level > 0) {
    // We only update this when the user accepts a subpopup suggestion since the
    // filling granularity doesn't change by accepting a top-level popup
    // suggestion but stays the same.
    last_accepted_address_suggestion_for_address_form_section_
        [autofill_trigger_field->section()] = type;
  }

  mojom::ActionPersistence action_persistence =
      is_preview ? mojom::ActionPersistence::kPreview
                 : mojom::ActionPersistence::kFill;

  PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
  const AutofillProfile* profile =
      type == SuggestionType::kDevtoolsTestAddressEntry
          ? GetTestAddressByGUID(
                manager_->client().GetTestAddresses(),
                absl::get<Suggestion::Guid>(backend_id).value())
          : pdm->address_data_manager().GetProfileByGUID(
                absl::get<Suggestion::Guid>(backend_id).value());
  if (profile) {
    manager_->FillOrPreviewProfileForm(action_persistence, query_form_,
                                       query_field_, *profile, trigger_details);
  } else if (const CreditCard* credit_card =
                 pdm->payments_data_manager().GetCreditCardByGUID(
                     absl::get<Suggestion::Guid>(backend_id).value())) {
    is_preview
        ? manager_->FillOrPreviewCreditCardForm(
              mojom::ActionPersistence::kPreview, query_form_, query_field_,
              *credit_card, std::u16string(), trigger_details)
        : manager_->AuthenticateThenFillCreditCardForm(
              query_form_, query_field_,
              type == SuggestionType::kVirtualCreditCardEntry
                  ? CreditCard::CreateVirtualCard(*credit_card)
                  : *credit_card,
              trigger_details);
  }
}

void AutofillExternalDelegate::PossiblyRemoveAutofillWarnings(
    std::vector<Suggestion>* suggestions) {
  while (suggestions->size() > 1 &&
         IsAutofillWarningEntry(suggestions->front().type) &&
         !IsAutofillWarningEntry(suggestions->back().type)) {
    // If we received warnings instead of suggestions from Autofill but regular
    // suggestions from autocomplete, don't show the Autofill warnings.
    suggestions->erase(suggestions->begin());
  }
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<Suggestion>* suggestions) {
  if (datalist_.empty()) {
    return;
  }

  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  auto datalist_values = base::MakeFlatSet<std::u16string>(
      datalist_, {}, [](const SelectOption& option) { return option.value; });
  std::erase_if(*suggestions, [&datalist_values](const Suggestion& suggestion) {
    return suggestion.type == SuggestionType::kAutocompleteEntry &&
           base::Contains(datalist_values, suggestion.main_text.value);
  });

#if !BUILDFLAG(IS_ANDROID)
  // Insert the separator between the datalist and Autofill/Autocomplete values
  // (if there are any).
  if (!suggestions->empty()) {
    suggestions->insert(suggestions->begin(),
                        Suggestion(SuggestionType::kSeparator));
  }
#endif

  // Insert the datalist elements at the beginning.
  suggestions->insert(suggestions->begin(), datalist_.size(), Suggestion());
  for (size_t i = 0; i < datalist_.size(); i++) {
    (*suggestions)[i].main_text =
        Suggestion::Text(datalist_[i].value, Suggestion::Text::IsPrimary(true));
    (*suggestions)[i].labels = {{Suggestion::Text(datalist_[i].text)}};
    (*suggestions)[i].type = SuggestionType::kDatalistEntry;
  }
}

bool AutofillExternalDelegate::IsPaymentsManualFallbackOnNonPaymentsField()
    const {
  if (trigger_source_ ==
      AutofillSuggestionTriggerSource::kManualFallbackPayments) {
    const AutofillField* field = GetQueriedAutofillField();
    return !field || !FieldTypeGroupSet({FieldTypeGroup::kCreditCard,
                                         FieldTypeGroup::kStandaloneCvcField})
                          .contains(field->Type().group());
  }
  return false;
}

AutofillSuggestionTriggerSource
AutofillExternalDelegate::GetReopenTriggerSource() const {
  // Manual fallbacks show suggestions of a specific type. If the Autofill
  // wasn't triggered manually, return
  // `kShowPromptAfterDialogClosedNonManualFallback` to avoid showing other
  // suggestion types.
  return IsAutofillManuallyTriggered(trigger_source_)
             ? trigger_source_
             : AutofillSuggestionTriggerSource::
                   kShowPromptAfterDialogClosedNonManualFallback;
}

void AutofillExternalDelegate::LogRankingContextAfterSuggestionAccepted(
    const Suggestion& accepted_suggestion) {
  CHECK(accepted_suggestion.type == SuggestionType::kCreditCardEntry);
  const Suggestion::Guid& suggestion_guid =
      accepted_suggestion.GetBackendId<Suggestion::Guid>();
  if (suggestion_ranking_context_ &&
      suggestion_ranking_context_->RankingsAreDifferent() &&
      suggestion_ranking_context_->suggestion_rankings_difference_map.contains(
          suggestion_guid)) {
    autofill_metrics::LogAutofillRankingSuggestionDifference(
        suggestion_ranking_context_->suggestion_rankings_difference_map
            .find(suggestion_guid)
            ->second);
  }
}

void AutofillExternalDelegate::DidAcceptAddressSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.Address",
      query_field_.value().size());
  switch (suggestion.type) {
    case SuggestionType::kAddressEntry:
      autofill_metrics::LogSuggestionAcceptedIndex(
          position.row,
          GetFillingProductFromSuggestionType(SuggestionType::kAddressEntry),
          manager_->client().IsOffTheRecord());
      ABSL_FALLTHROUGH_INTENDED;
    case SuggestionType::kFillEverythingFromAddressProfile:
      autofill_metrics::LogFillingMethodUsed(
          FillingMethod::kFullForm, FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          position, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kFillFullAddress:
    case SuggestionType::kFillFullName:
    case SuggestionType::kFillFullEmail:
    case SuggestionType::kFillFullPhoneNumber: {
      FillingMethod filling_method =
          GetFillingMethodFromSuggestionType(suggestion.type);
      autofill_metrics::LogFillingMethodUsed(
          filling_method, FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          position, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetTargetFieldTypesFromFillingMethod(filling_method)});
      break;
    }
    case SuggestionType::kAddressFieldByFieldFilling:
      FillFieldByFieldFillingSuggestion(suggestion, position);
      break;
    case SuggestionType::kEditAddressProfile:
      ShowEditAddressProfileDialog(
          suggestion.GetBackendId<Suggestion::Guid>().value());
      break;
    case SuggestionType::kDeleteAddressProfile:
      ShowDeleteAddressProfileDialog(
          suggestion.GetBackendId<Suggestion::Guid>().value());
      break;
    case SuggestionType::kDevtoolsTestAddressEntry: {
      const AutofillProfile* profile = GetTestAddressByGUID(
          manager_->client().GetTestAddresses(),
          absl::get<Suggestion::Guid>(
              suggestion.GetPayload<Suggestion::BackendId>())
              .value());
      CHECK(profile);
      autofill_metrics::OnDevtoolsTestAddressesAccepted(
          profile->GetInfo(ADDRESS_HOME_COUNTRY, "en-US"));
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          position, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    }
    default:
      NOTREACHED();  // Should be handled elsewhere.
  }
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  // The user having accepted an address suggestion on this field, all strikes
  // previously recorded for this field are cleared so that address suggestions
  // can be automatically shown again if needed.
  manager_->client()
      .GetPersonalDataManager()
      ->address_data_manager()
      .ClearStrikesToBlockAddressSuggestions(
          CalculateFormSignature(query_form_),
          CalculateFieldSignatureForField(query_field_), query_form_.url());
#endif
}

void AutofillExternalDelegate::DidAcceptPaymentsSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  base::UmaHistogramCounts100(
      "Autofill.Suggestion.AcceptanceFieldValueLength.CreditCard",
      query_field_.value().size());
  switch (suggestion.type) {
    case SuggestionType::kCreditCardEntry:
      autofill_metrics::LogSuggestionAcceptedIndex(
          position.row,
          GetFillingProductFromSuggestionType(SuggestionType::kCreditCardEntry),
          manager_->client().IsOffTheRecord());
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableRankingFormulaCreditCards)) {
        LogRankingContextAfterSuggestionAccepted(suggestion);
      }
      FillAutofillFormData(
          suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
          position, /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case SuggestionType::kVirtualCreditCardEntry:
      if (IsPaymentsManualFallbackOnNonPaymentsField()) {
        if (const CreditCard* credit_card =
                manager_->client()
                    .GetPersonalDataManager()
                    ->payments_data_manager()
                    .GetCreditCardByGUID(
                        suggestion.GetBackendId<Suggestion::Guid>().value())) {
          CreditCard virtual_card = CreditCard::CreateVirtualCard(*credit_card);
          manager_->GetCreditCardAccessManager().FetchCreditCard(
              &virtual_card,
              base::BindOnce(
                  &AutofillExternalDelegate::OnVirtualCreditCardFetched,
                  GetWeakPtr()));
        }
      } else {
        // There can be multiple virtual credit cards that all rely on
        // SuggestionType::kVirtualCreditCardEntry as a `type`.
        // In this case, the payload contains the backend id, which is a GUID
        // that identifies the actually chosen credit card.
        FillAutofillFormData(
            suggestion.type, suggestion.GetPayload<Suggestion::BackendId>(),
            position, /*is_preview=*/false,
            {.trigger_source =
                 TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      }
      break;
    case SuggestionType::kCreditCardFieldByFieldFilling:
      FillFieldByFieldFillingSuggestion(suggestion, position);
      break;
    case SuggestionType::kIbanEntry:
      // User chooses an IBAN suggestion and if it is a local IBAN, full IBAN
      // value will directly populate the IBAN field. In the case of a server
      // IBAN, a request to unmask the IBAN will be sent to the GPay server, and
      // the IBAN value will be filled if the request is successful.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->GetIbanAccessManager()
          ->FetchValue(suggestion.GetPayload<Suggestion::BackendId>(),
                       base::BindOnce(
                           [](base::WeakPtr<AutofillExternalDelegate> delegate,
                              const std::u16string& value) {
                             if (delegate) {
                               delegate->manager_->FillOrPreviewField(
                                   mojom::ActionPersistence::kFill,
                                   mojom::FieldActionType::kReplaceAll,
                                   delegate->query_form_,
                                   delegate->query_field_, value,
                                   SuggestionType::kIbanEntry, IBAN_VALUE);
                             }
                           },
                           GetWeakPtr()));
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          suggestion.type, MERCHANT_PROMO_CODE);
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kSeePromoCodeDetails:
      // Open a new tab and navigate to the offer details page.
      manager_->client()
          .GetPaymentsAutofillClient()
          ->OpenPromoCodeOfferDetailsURL(suggestion.GetPayload<GURL>());
      manager_->OnSingleFieldSuggestionSelected(suggestion, query_form_,
                                                query_field_);
      break;
    case SuggestionType::kShowAccountCards:
      autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
          autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
              kButtonClicked);
      manager_->OnUserAcceptedCardsFromAccountOption();
      break;
    case SuggestionType::kScanCreditCard:
      manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardScanned,
                         GetWeakPtr(),
                         AutofillTriggerSource::kKeyboardAccessory));
      break;
    default:
      NOTREACHED();  // Should be handled elsewhere
  }
  if (base::Contains(shown_suggestion_types_,
                     SuggestionType::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        suggestion.type == SuggestionType::kScanCreditCard
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }
}

}  // namespace autofill
