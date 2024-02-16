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
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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
#include "components/autofill/core/browser/metrics/address_rewriter_in_profile_subset_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/platform/ax_platform.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Returns true if the suggestion entry is an Autofill warning message.
// Warning messages should display on top of suggestion list.
bool IsAutofillWarningEntry(PopupItemId popup_item_id) {
  return popup_item_id == PopupItemId::kInsecureContextPaymentDisabledMessage ||
         popup_item_id == PopupItemId::kMixedFormMessage;
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
      // On Android, no popup exists. Instead, the keyboard accessory is used.
#if BUILDFLAG(IS_ANDROID)
      return AutofillTriggerSource::kKeyboardAccessory;
#else
      return AutofillTriggerSource::kPopup;
#endif  // BUILDFLAG(IS_ANDROID)
    case AutofillSuggestionTriggerSource::kManualFallbackAddress:
    case AutofillSuggestionTriggerSource::kManualFallbackPayments:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
      // Manual fallbacks are both a suggestion trigger source (e.g. through the
      // context menu) and a trigger source (by selecting a suggestion generated
      // through the context menu).
      return AutofillTriggerSource::kManualFallback;
  }
  NOTREACHED_NORETURN();
}
}  // namespace

AutofillExternalDelegate::AutofillExternalDelegate(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

AutofillExternalDelegate::~AutofillExternalDelegate() {
  if (deletion_callback_)
    std::move(deletion_callback_).Run();
}

// static
bool AutofillExternalDelegate::IsAutofillAndFirstLayerSuggestionId(
    PopupItemId item_id) {
  switch (item_id) {
    case PopupItemId::kAddressEntry:
    case PopupItemId::kFillFullAddress:
    case PopupItemId::kAddressFieldByFieldFilling:
    case PopupItemId::kCreditCardFieldByFieldFilling:
    case PopupItemId::kFillFullName:
    case PopupItemId::kFillFullPhoneNumber:
    case PopupItemId::kFillFullEmail:
    case PopupItemId::kCreditCardEntry:
    case PopupItemId::kDevtoolsTestAddresses:
      // Virtual cards can appear on their own when filling the CVC for a card
      // that a merchant has saved. This indicates there could be Autofill
      // suggestions related to standalone CVC fields.
    case PopupItemId::kVirtualCreditCardEntry:
      return true;
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kAutocompleteEntry:
    case PopupItemId::kAutofillOptions:
    case PopupItemId::kClearForm:
    case PopupItemId::kCompose:
    case PopupItemId::kCreateNewPlusAddress:
    case PopupItemId::kDatalistEntry:
    case PopupItemId::kDeleteAddressProfile:
    case PopupItemId::kDevtoolsTestAddressEntry:
    case PopupItemId::kEditAddressProfile:
    case PopupItemId::kFillEverythingFromAddressProfile:
    case PopupItemId::kFillExistingPlusAddress:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kIbanEntry:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
    case PopupItemId::kMerchantPromoCodeEntry:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kScanCreditCard:
    case PopupItemId::kSeePromoCodeDetails:
    case PopupItemId::kSeparator:
    case PopupItemId::kShowAccountCards:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
    case PopupItemId::kPasswordFieldByFieldFilling:
    case PopupItemId::kFillPassword:
    case PopupItemId::kViewPasswordDetails:
      return false;
  }
}

void AutofillExternalDelegate::OnQuery(
    const FormData& form,
    const FormFieldData& field,
    const gfx::RectF& element_bounds,
    AutofillSuggestionTriggerSource trigger_source) {
  query_form_ = form;
  query_field_ = field;
  element_bounds_ = element_bounds;
  trigger_source_ = trigger_source;
}

const AutofillField* AutofillExternalDelegate::GetQueriedAutofillField() const {
  return manager_->GetAutofillField(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions) {
  if (field_id != query_field_.global_id()) {
    return;
  }

#if BUILDFLAG(IS_IOS)
  if (!manager_->client().IsLastQueriedField(field_id)) {
    return;
  }
#endif

  std::vector<Suggestion> suggestions(input_suggestions);

  // Hide warnings as appropriate.
  PossiblyRemoveAutofillWarnings(&suggestions);

  // TODO(b/320126773): consider moving these metrics to a better place.
  if (base::ranges::any_of(suggestions, [](const Suggestion& suggestion) {
        return suggestion.popup_item_id == PopupItemId::kShowAccountCards;
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
    manager_->client().HideAutofillPopup(PopupHidingReason::kNoSuggestions);
    return;
  }

  shown_suggestion_types_.clear();
  for (const Suggestion& suggestion : input_suggestions) {
    shown_suggestion_types_.push_back(suggestion.popup_item_id);
  }
  // Send to display.
  if (query_field_.is_focusable && manager_->driver().CanShowAutofillUi()) {
    AutofillClient::PopupOpenArgs open_args(element_bounds_,
                                            query_field_.text_direction,
                                            suggestions, trigger_source_);
    manager_->client().ShowAutofillPopup(open_args, GetWeakPtr());
  }
}

std::optional<FieldTypeSet>
AutofillExternalDelegate::GetLastFieldTypesToFillForSection(
    const Section& section) const {
  if (auto it =
          last_field_types_to_fill_for_address_form_section_.find(section);
      it != last_field_types_to_fill_for_address_form_section_.end()) {
    return it->second;
  }
  return std::nullopt;
}

bool AutofillExternalDelegate::HasActiveScreenReader() const {
  // Note: This always returns false if ChromeVox is in use because the
  // process-wide AXMode is not updated in that case; except for Lacros, where
  // kScreenReader mirrors the spoken feedback preference.
  return ui::AXPlatform::GetInstance().GetMode().has_mode(
      ui::AXMode::kScreenReader);
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
  manager_->client().UpdateAutofillPopupDataListValues(datalist_);
}

void AutofillExternalDelegate::OnPopupShown() {
  // Popups are expected to be Autofill or Autocomplete.
  DCHECK_NE(GetMainFillingProduct(), FillingProduct::kPassword);

  const bool has_autofill_suggestions = base::ranges::any_of(
      shown_suggestion_types_, IsAutofillAndFirstLayerSuggestionId);
  if (has_autofill_suggestions) {
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutofillAvailable);
  } else {
    // We send autocomplete availability event even though there might be no
    // autocomplete suggestions shown.
    // TODO(b/315748930): Provide AX event only for autocomplete entries.
    OnAutofillAvailabilityEvent(
        mojom::AutofillSuggestionAvailability::kAutocompleteAvailable);
    if (base::Contains(shown_suggestion_types_,
                       PopupItemId::kAutocompleteEntry)) {
      AutofillMetrics::OnAutocompleteSuggestionsShown();
    }
  }

  manager_->DidShowSuggestions(shown_suggestion_types_, query_form_,
                               query_field_);

  if (base::Contains(shown_suggestion_types_, PopupItemId::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
  }
}

void AutofillExternalDelegate::OnPopupHidden() {
  manager_->OnPopupHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const Suggestion& suggestion) {
  if (!suggestion.is_acceptable) {
    // TODO(crbug.com/1493361): Handle this in the popup controller.
    return;
  }
  ClearPreviewedForm();

  const Suggestion::BackendId backend_id =
      suggestion.GetPayload<Suggestion::BackendId>();

  switch (suggestion.popup_item_id) {
    case PopupItemId::kClearForm:
      if (base::FeatureList::IsEnabled(features::kAutofillUndo)) {
        manager_->UndoAutofill(mojom::ActionPersistence::kPreview, query_form_,
                               query_field_);
      }
      break;
    case PopupItemId::kAddressEntry:
    case PopupItemId::kCreditCardEntry:
    case PopupItemId::kFillEverythingFromAddressProfile:
      FillAutofillFormData(
          suggestion.popup_item_id, backend_id, true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case PopupItemId::kFillFullAddress:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetAddressFieldsForGroupFilling()});
      break;
    case PopupItemId::kFillFullName:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kName)});
      break;
    case PopupItemId::kFillFullPhoneNumber:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kPhone)});
      break;
    case PopupItemId::kFillFullEmail:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kEmail)});
      break;
    case PopupItemId::kAutocompleteEntry:
    case PopupItemId::kIbanEntry:
    case PopupItemId::kMerchantPromoCodeEntry:
    case PopupItemId::kFillExistingPlusAddress:
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kPreview,
          mojom::TextReplacement::kReplaceAll, query_form_, query_field_,
          suggestion.main_text.value, suggestion.popup_item_id);
      break;
    case PopupItemId::kAddressFieldByFieldFilling:
    case PopupItemId::kCreditCardFieldByFieldFilling:
      PreviewFieldByFieldFillingSuggestion(suggestion);
      break;
    case PopupItemId::kVirtualCreditCardEntry:
      // If triggered on a non payments form, don't preview the value.
      if (IsPaymentsManualFallbackOnNonPaymentsField()) {
        break;
      }
      FillAutofillFormData(
          suggestion.popup_item_id, backend_id, /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case PopupItemId::kEditAddressProfile:
    case PopupItemId::kDeleteAddressProfile:
    case PopupItemId::kAutofillOptions:
    case PopupItemId::kCompose:
    case PopupItemId::kDatalistEntry:
    case PopupItemId::kShowAccountCards:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
    case PopupItemId::kScanCreditCard:
    case PopupItemId::kCreateNewPlusAddress:
    case PopupItemId::kSeePromoCodeDetails:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kDevtoolsTestAddresses:
    case PopupItemId::kDevtoolsTestAddressEntry:
      break;
    case PopupItemId::kSeparator:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
    case PopupItemId::kPasswordFieldByFieldFilling:
    case PopupItemId::kFillPassword:
    case PopupItemId::kViewPasswordDetails:
      NOTREACHED_NORETURN();  // Should be handled elsewhere.
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position) {
  if (!suggestion.is_acceptable) {
    // TODO(crbug.com/1493361): Handle this in the popup controller.
    return;
  }
  switch (suggestion.popup_item_id) {
    case PopupItemId::kAutofillOptions: {
      // User selected 'Autofill Options'.
      const FillingProduct main_filling_product = GetMainFillingProduct();
      CHECK(main_filling_product == FillingProduct::kAddress ||
            main_filling_product == FillingProduct::kCreditCard ||
            main_filling_product == FillingProduct::kIban);
      autofill_metrics::LogAutofillSelectedManageEntry(main_filling_product);
      manager_->client().ShowAutofillSettings(main_filling_product);
      break;
    }
    case PopupItemId::kEditAddressProfile: {
      ShowEditAddressProfileDialog(
          suggestion.GetBackendId<Suggestion::Guid>().value());
      break;
    }
    case PopupItemId::kDeleteAddressProfile:
      ShowDeleteAddressProfileDialog(
          suggestion.GetBackendId<Suggestion::Guid>().value());
      break;
    case PopupItemId::kClearForm:
      // This serves as a clear form or undo autofill suggestion, depending on
      // the state of the feature `kAutofillUndo`.
      if (base::FeatureList::IsEnabled(features::kAutofillUndo)) {
        manager_->UndoAutofill(mojom::ActionPersistence::kFill, query_form_,
                               query_field_);
      } else {
        // User selected 'Clear form'.
        AutofillMetrics::LogAutofillFormCleared();
        manager_->driver().RendererShouldClearFilledSection();
      }
      break;
    case PopupItemId::kDatalistEntry:
      manager_->driver().RendererShouldAcceptDataListSuggestion(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case PopupItemId::kAddressFieldByFieldFilling:
    case PopupItemId::kCreditCardFieldByFieldFilling:
      FillFieldByFieldFillingSuggestion(suggestion, position, trigger_source_);
      break;
    case PopupItemId::kIbanEntry:
      // User chooses an IBAN suggestion and if it is a local IBAN, full IBAN
      // value will directly populate the IBAN field. In the case of a server
      // IBAN, a request to unmask the IBAN will be sent to the GPay server, and
      // the IBAN value will be filled if the request is successful.
      manager_->client().GetIbanAccessManager()->FetchValue(
          suggestion, base::BindOnce(
                          [](base::WeakPtr<AutofillExternalDelegate> delegate,
                             const std::u16string& value) {
                            if (delegate) {
                              delegate->manager_->FillOrPreviewField(
                                  mojom::ActionPersistence::kFill,
                                  mojom::TextReplacement::kReplaceAll,
                                  delegate->query_form_, delegate->query_field_,
                                  value, PopupItemId::kIbanEntry);
                            }
                          },
                          GetWeakPtr()));
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.popup_item_id,
                                                query_form_, query_field_);
      break;
    case PopupItemId::kFillFullAddress:
      autofill_metrics::LogFillingMethodUsed(
          autofill_metrics::AutofillFillingMethodMetric::kGroupFillingAddress,
          FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetAddressFieldsForGroupFilling()});
      break;
    case PopupItemId::kFillFullName:
      autofill_metrics::LogFillingMethodUsed(
          autofill_metrics::AutofillFillingMethodMetric::kGroupFillingName,
          FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kName)});
      break;
    case PopupItemId::kFillFullPhoneNumber:
      autofill_metrics::LogFillingMethodUsed(
          autofill_metrics::AutofillFillingMethodMetric::
              kGroupFillingPhoneNumber,
          FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kPhone)});
      break;
    case PopupItemId::kFillFullEmail:
      autofill_metrics::LogFillingMethodUsed(
          autofill_metrics::AutofillFillingMethodMetric::kGroupFillingEmail,
          FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_),
           .field_types_to_fill =
               GetFieldTypesOfGroup(FieldTypeGroup::kEmail)});
      break;
    case PopupItemId::kAutocompleteEntry:
      AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(position.row);
      ABSL_FALLTHROUGH_INTENDED;
    case PopupItemId::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          suggestion.popup_item_id);
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.popup_item_id,
                                                query_form_, query_field_);
      break;
    case PopupItemId::kScanCreditCard:
      manager_->client().ScanCreditCard(base::BindOnce(
          &AutofillExternalDelegate::OnCreditCardScanned, GetWeakPtr(),
          AutofillTriggerSource::kKeyboardAccessory));
      break;
    case PopupItemId::kShowAccountCards:
      autofill_metrics::LogAutofillShowCardsFromGoogleAccountButtonEventMetric(
          autofill_metrics::ShowCardsFromGoogleAccountButtonEvent::
              kButtonClicked);
      manager_->OnUserAcceptedCardsFromAccountOption();
      break;
    case PopupItemId::kVirtualCreditCardEntry:
      if (IsPaymentsManualFallbackOnNonPaymentsField()) {
        if (const CreditCard* credit_card =
                manager_->client()
                    .GetPersonalDataManager()
                    ->GetCreditCardByGUID(
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
        // PopupItemId::kVirtualCreditCardEntry as a `popup_item_id`. In this
        // case, the payload contains the backend id, which is a GUID that
        // identifies the actually chosen credit card.
        FillAutofillFormData(
            suggestion.popup_item_id,
            suggestion.GetPayload<Suggestion::BackendId>(),
            /*is_preview=*/false,
            {.trigger_source =
                 TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      }
      break;
    case PopupItemId::kSeePromoCodeDetails:
      // Open a new tab and navigate to the offer details page.
      manager_->client().OpenPromoCodeOfferDetailsURL(
          suggestion.GetPayload<GURL>());
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.popup_item_id,
                                                query_form_, query_field_);
      break;
    case PopupItemId::kFillExistingPlusAddress:
      if (AutofillPlusAddressDelegate* plus_address_delegate =
              manager_->client().GetPlusAddressDelegate()) {
        plus_address_delegate->RecordAutofillSuggestionEvent(
            AutofillPlusAddressDelegate::SuggestionEvent::
                kExistingPlusAddressChosen);
      }
      manager_->FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
          query_form_, query_field_, suggestion.main_text.value,
          PopupItemId::kFillExistingPlusAddress);
      break;
    case PopupItemId::kCreateNewPlusAddress: {
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
                  mojom::TextReplacement::kReplaceAll, form, field,
                  base::UTF8ToUTF16(plus_address),
                  PopupItemId::kCreateNewPlusAddress);
            }
          },
          GetWeakPtr(), query_form_, query_field_);
      manager_->client().OfferPlusAddressCreation(
          manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
          std::move(callback));
      break;
    }
    case PopupItemId::kCompose:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        delegate->OpenCompose(
            manager_->driver(), query_field_.renderer_form_id(),
            query_field_.global_id(),
            autofill::AutofillComposeDelegate::UiEntryPoint::kAutofillPopup);
      }
      break;
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
    case PopupItemId::kMixedFormMessage:
      // If the selected element is a warning we don't want to do anything.
      break;
    case PopupItemId::kAddressEntry:
      autofill_metrics::LogAutofillSuggestionAcceptedIndex(
          position.row,
          GetFillingProductFromPopupItemId(PopupItemId::kAddressEntry),
          manager_->client().IsOffTheRecord());
      autofill_metrics::LogFillingMethodUsed(
          autofill_metrics::AutofillFillingMethodMetric::kFullForm,
          FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      autofill_metrics::LogUserAcceptedPreviouslyHiddenProfileSuggestion(
          suggestion.hidden_prior_to_address_rewriter_usage);
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case PopupItemId::kFillEverythingFromAddressProfile:
      autofill_metrics::LogFillingMethodUsed(
          autofill_metrics::AutofillFillingMethodMetric::kFullForm,
          FillingProduct::kAddress,
          /*triggering_field_type_matches_filling_product=*/true);
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case PopupItemId::kCreditCardEntry:
      autofill_metrics::LogAutofillSuggestionAcceptedIndex(
          position.row,
          GetFillingProductFromPopupItemId(PopupItemId::kCreditCardEntry),
          manager_->client().IsOffTheRecord());
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case PopupItemId::kDevtoolsTestAddresses:
    case PopupItemId::kDevtoolsTestAddressEntry:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source_)});
      break;
    case PopupItemId::kSeparator:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
    case PopupItemId::kPasswordFieldByFieldFilling:
    case PopupItemId::kFillPassword:
    case PopupItemId::kViewPasswordDetails:
      NOTREACHED_NORETURN();  // Should be handled elsewhere.
  }

  if (base::Contains(shown_suggestion_types_, PopupItemId::kScanCreditCard)) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        suggestion.popup_item_id == PopupItemId::kScanCreditCard
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }

  if (suggestion.popup_item_id == PopupItemId::kShowAccountCards) {
    manager_->RefetchCardsAndUpdatePopup(query_form_, query_field_,
                                         element_bounds_);
  } else {
    manager_->client().HideAutofillPopup(PopupHidingReason::kAcceptSuggestion);
  }
}

void AutofillExternalDelegate::DidPerformButtonActionForSuggestion(
    const Suggestion& suggestion) {
  switch (suggestion.popup_item_id) {
    case PopupItemId::kCompose:
      NOTIMPLEMENTED();
      return;
    default:
      NOTREACHED();
  }
}

bool AutofillExternalDelegate::RemoveSuggestion(const Suggestion& suggestion) {
  switch (suggestion.popup_item_id) {
    // These PopupItemIds are various types which can appear in the first level
    // suggestion to fill an address or credit card field.
    case PopupItemId::kAddressEntry:
    case PopupItemId::kFillFullAddress:
    case PopupItemId::kFillFullName:
    case PopupItemId::kFillFullEmail:
    case PopupItemId::kFillFullPhoneNumber:
    case PopupItemId::kAddressFieldByFieldFilling:
    case PopupItemId::kCreditCardFieldByFieldFilling:
    case PopupItemId::kCreditCardEntry:
      return manager_->RemoveAutofillProfileOrCreditCard(
          suggestion.GetPayload<Suggestion::BackendId>());
    case PopupItemId::kAutocompleteEntry:
      manager_->RemoveCurrentSingleFieldSuggestion(query_field_.name,
                                                   suggestion.main_text.value,
                                                   suggestion.popup_item_id);
      return true;
    case PopupItemId::kFillEverythingFromAddressProfile:
    case PopupItemId::kEditAddressProfile:
    case PopupItemId::kDeleteAddressProfile:
    case PopupItemId::kAutofillOptions:
    case PopupItemId::kCreateNewPlusAddress:
    case PopupItemId::kFillExistingPlusAddress:
    case PopupItemId::kInsecureContextPaymentDisabledMessage:
    case PopupItemId::kScanCreditCard:
    case PopupItemId::kVirtualCreditCardEntry:
    case PopupItemId::kIbanEntry:
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kAllSavedPasswordsEntry:
    case PopupItemId::kGeneratePasswordEntry:
    case PopupItemId::kShowAccountCards:
    case PopupItemId::kPasswordAccountStorageOptIn:
    case PopupItemId::kPasswordAccountStorageOptInAndGenerate:
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kPasswordAccountStorageReSignin:
    case PopupItemId::kPasswordAccountStorageEmpty:
    case PopupItemId::kCompose:
    case PopupItemId::kDatalistEntry:
    case PopupItemId::kMerchantPromoCodeEntry:
    case PopupItemId::kSeePromoCodeDetails:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
    case PopupItemId::kSeparator:
    case PopupItemId::kClearForm:
    case PopupItemId::kMixedFormMessage:
    case PopupItemId::kDevtoolsTestAddresses:
    case PopupItemId::kDevtoolsTestAddressEntry:
    case PopupItemId::kPasswordFieldByFieldFilling:
    case PopupItemId::kFillPassword:
    case PopupItemId::kViewPasswordDetails:
      return false;
  }
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client().HideAutofillPopup(PopupHidingReason::kEndEditing);
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  manager_->driver().RendererShouldClearPreviewedForm();
}

FillingProduct AutofillExternalDelegate::GetMainFillingProduct() const {
  for (PopupItemId popup_item_id : shown_suggestion_types_) {
    if (FillingProduct product =
            GetFillingProductFromPopupItemId(popup_item_id);
        product != FillingProduct::kNone) {
      return product;
    }
  }
  return FillingProduct::kNone;
}

int32_t AutofillExternalDelegate::GetWebContentsPopupControllerAxId() const {
  return query_field_.form_control_ax_id;
}

void AutofillExternalDelegate::RegisterDeletionCallback(
    base::OnceClosure deletion_callback) {
  deletion_callback_ = std::move(deletion_callback);
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::ShowEditAddressProfileDialog(
    const std::string& guid) {
  AutofillProfile* profile =
      manager_->client().GetPersonalDataManager()->GetProfileByGUID(guid);
  if (profile) {
    manager_->client().ShowEditAddressProfileDialog(
        *profile,
        base::BindOnce(&AutofillExternalDelegate::OnAddressEditorClosed,
                       GetWeakPtr()));
  }
}

void AutofillExternalDelegate::ShowDeleteAddressProfileDialog(
    const std::string& guid) {
  AutofillProfile* profile =
      manager_->client().GetPersonalDataManager()->GetProfileByGUID(guid);
  if (profile) {
    manager_->client().ShowDeleteAddressProfileDialog(
        *profile,
        base::BindOnce(&AutofillExternalDelegate::OnDeleteDialogClosed,
                       GetWeakPtr(), guid));
  }
}

void AutofillExternalDelegate::OnAddressEditorClosed(
    AutofillClient::SaveAddressProfileOfferUserDecision decision,
    base::optional_ref<const AutofillProfile> edited_profile) {
  if (decision ==
      AutofillClient::SaveAddressProfileOfferUserDecision::kEditAccepted) {
    autofill_metrics::LogEditAddressProfileDialogClosed(
        /*user_saved_changes=*/true);
    PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
    if (!pdm_observation_.IsObserving()) {
      pdm_observation_.Observe(pdm);
    }
    CHECK(edited_profile.has_value());
    pdm->UpdateProfile(edited_profile.value());
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
    PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
    if (!pdm_observation_.IsObserving()) {
      pdm_observation_.Observe(pdm);
    }
    pdm->RemoveByGUID(guid);
    return;
  }
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnPersonalDataChanged() {
  pdm_observation_.Reset();
  manager_->driver().RendererShouldTriggerSuggestions(query_field_.global_id(),
                                                      GetReopenTriggerSource());
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillCreditCardForm(query_form_, query_field_, card,
                               std::u16string(),
                               {.trigger_source = trigger_source});
}

void AutofillExternalDelegate::PreviewFieldByFieldFillingSuggestion(
    const Suggestion& suggestion) {
  CHECK(suggestion.popup_item_id == PopupItemId::kAddressFieldByFieldFilling ||
        suggestion.popup_item_id ==
            PopupItemId::kCreditCardFieldByFieldFilling);
  CHECK(suggestion.field_by_field_filling_type_used);
  const auto guid = suggestion.GetBackendId<Suggestion::Guid>().value();
  if (const AutofillProfile* profile =
          manager_->client().GetPersonalDataManager()->GetProfileByGUID(guid)) {
    PreviewAddressFieldByFieldFillingSuggestion(*profile, suggestion);
  } else if (manager_->client().GetPersonalDataManager()->GetCreditCardByGUID(
                 guid)) {
    PreviewCreditCardFieldByFieldFillingSuggestion(suggestion);
  }
}

void AutofillExternalDelegate::FillFieldByFieldFillingSuggestion(
    const Suggestion& suggestion,
    const SuggestionPosition& position,
    AutofillSuggestionTriggerSource trigger_source) {
  CHECK(suggestion.popup_item_id == PopupItemId::kAddressFieldByFieldFilling ||
        suggestion.popup_item_id ==
            PopupItemId::kCreditCardFieldByFieldFilling);
  CHECK(suggestion.field_by_field_filling_type_used);
  const auto guid = suggestion.GetBackendId<Suggestion::Guid>().value();
  if (const AutofillProfile* profile =
          manager_->client().GetPersonalDataManager()->GetProfileByGUID(guid)) {
    FillAddressFieldByFieldFillingSuggestion(*profile, suggestion, position,
                                             trigger_source);
  } else if (const CreditCard* credit_card = manager_->client()
                                                 .GetPersonalDataManager()
                                                 ->GetCreditCardByGUID(guid)) {
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
        mojom::ActionPersistence::kPreview, mojom::TextReplacement::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.popup_item_id);
  }
}

void AutofillExternalDelegate::FillAddressFieldByFieldFillingSuggestion(
    const AutofillProfile& profile,
    const Suggestion& suggestion,
    const SuggestionPosition& position,
    AutofillSuggestionTriggerSource trigger_source) {
  const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
  if (autofill_trigger_field) {
    // We target only the triggering field type in the field-by-field filling
    // case.
    // Note that, we only use
    // `last_field_types_to_fill_for_address_form_section_` to know the current
    // filling granularity. The exact type is not important, what matters here
    // is that the user targeted one ONE field, i.e, field-by-field filling.
    last_field_types_to_fill_for_address_form_section_[autofill_trigger_field
                                                           ->section] = {
        *suggestion.field_by_field_filling_type_used};
  }
  const bool is_triggering_field_address =
      autofill_trigger_field &&
      IsAddressType(autofill_trigger_field->Type().GetStorableType());

  autofill_metrics::LogFillingMethodUsed(
      autofill_metrics::AutofillFillingMethodMetric::kFieldByFieldFilling,
      FillingProduct::kAddress,
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
        mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
        query_form_, query_field_, filling_value, suggestion.popup_item_id);
  }
}

void AutofillExternalDelegate::PreviewCreditCardFieldByFieldFillingSuggestion(
    const Suggestion& suggestion) {
  manager_->FillOrPreviewField(mojom::ActionPersistence::kPreview,
                               mojom::TextReplacement::kReplaceAll, query_form_,
                               query_field_, suggestion.main_text.value,
                               suggestion.popup_item_id);
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
  manager_->FillOrPreviewField(mojom::ActionPersistence::kFill,
                               mojom::TextReplacement::kReplaceAll, query_form_,
                               query_field_, suggestion.main_text.value,
                               suggestion.popup_item_id);
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
      mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
      query_form_, query_field_,
      credit_card->GetInfo(CREDIT_CARD_NUMBER, manager_->app_locale()),
      PopupItemId::kCreditCardFieldByFieldFilling);
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
    PopupItemId popup_item_id,
    Suggestion::BackendId backend_id,
    bool is_preview,
    const AutofillTriggerDetails& trigger_details) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillGranularFillingAvailable)) {
    // Only address suggestions store the last field types to
    // fill. This is because this is the only use case where filling
    // granularies need to be persisted.
    static constexpr auto kAutofillAddressSuggestions =
        base::MakeFixedFlatSet<PopupItemId>(
            {PopupItemId::kAddressEntry, PopupItemId::kFillFullAddress,
             PopupItemId::kFillFullPhoneNumber, PopupItemId::kFillFullEmail,
             PopupItemId::kFillFullName,
             PopupItemId::kFillEverythingFromAddressProfile});
    const AutofillField* autofill_trigger_field = GetQueriedAutofillField();
    if (autofill_trigger_field &&
        kAutofillAddressSuggestions.contains(popup_item_id) && !is_preview) {
      last_field_types_to_fill_for_address_form_section_[autofill_trigger_field
                                                             ->section] =
          trigger_details.field_types_to_fill;
    }
  }

  mojom::ActionPersistence action_persistence =
      is_preview ? mojom::ActionPersistence::kPreview
                 : mojom::ActionPersistence::kFill;

  PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
  if (CreditCard* credit_card = pdm->GetCreditCardByGUID(
          absl::get<Suggestion::Guid>(backend_id).value())) {
    if (popup_item_id == PopupItemId::kVirtualCreditCardEntry) {
      // Virtual credit cards are not persisted in Chrome, modify record type
      // locally.
      manager_->FillOrPreviewCreditCardForm(
          action_persistence, query_form_, query_field_,
          CreditCard::CreateVirtualCard(*credit_card), trigger_details);
    } else {
      manager_->FillOrPreviewCreditCardForm(action_persistence, query_form_,
                                            query_field_, *credit_card,
                                            trigger_details);
    }
  } else if (const AutofillProfile* profile = pdm->GetProfileByGUID(
                 absl::get<Suggestion::Guid>(backend_id).value())) {
    manager_->FillOrPreviewProfileForm(action_persistence, query_form_,
                                       query_field_, *profile, trigger_details);
  }
}

void AutofillExternalDelegate::PossiblyRemoveAutofillWarnings(
    std::vector<Suggestion>* suggestions) {
  while (suggestions->size() > 1 &&
         IsAutofillWarningEntry(suggestions->front().popup_item_id) &&
         !IsAutofillWarningEntry(suggestions->back().popup_item_id)) {
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
    return suggestion.popup_item_id == PopupItemId::kAutocompleteEntry &&
           base::Contains(datalist_values, suggestion.main_text.value);
  });

#if !BUILDFLAG(IS_ANDROID)
  // Insert the separator between the datalist and Autofill/Autocomplete values
  // (if there are any).
  if (!suggestions->empty()) {
    suggestions->insert(suggestions->begin(),
                        Suggestion(PopupItemId::kSeparator));
  }
#endif

  // Insert the datalist elements at the beginning.
  suggestions->insert(suggestions->begin(), datalist_.size(), Suggestion());
  for (size_t i = 0; i < datalist_.size(); i++) {
    (*suggestions)[i].main_text =
        Suggestion::Text(datalist_[i].value, Suggestion::Text::IsPrimary(true));
    (*suggestions)[i].labels = {{Suggestion::Text(datalist_[i].content)}};
    (*suggestions)[i].popup_item_id = PopupItemId::kDatalistEntry;
  }
}

bool AutofillExternalDelegate::IsPaymentsManualFallbackOnNonPaymentsField()
    const {
  if (trigger_source_ ==
      AutofillSuggestionTriggerSource::kManualFallbackPayments) {
    const AutofillField* field = GetQueriedAutofillField();
    return !field || field->Type().group() != FieldTypeGroup::kCreditCard;
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

}  // namespace autofill
