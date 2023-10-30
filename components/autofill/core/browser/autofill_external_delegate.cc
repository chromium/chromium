// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
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
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/address_rewriter_in_profile_subset_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
#include "ui/native_theme/native_theme.h"  // nogncheck
#endif

namespace autofill {

namespace {

// Returns true if the suggestion entry is an Autofill warning message.
// Warning messages should display on top of suggestion list.
bool IsAutofillWarningEntry(PopupItemId popup_item_id) {
  return popup_item_id == PopupItemId::kInsecureContextPaymentDisabledMessage ||
         popup_item_id == PopupItemId::kMixedFormMessage;
}

// Returns true if `item_id` identifies a suggestion which can appear on the
// first layer of the Autofill popup and can fill form fields.
bool IsFirstLayerFormFillingSuggestionId(PopupItemId item_id) {
  switch (item_id) {
    case PopupItemId::kAddressEntry:
    case PopupItemId::kFillFullAddress:
    case PopupItemId::kFieldByFieldFilling:
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
    case PopupItemId::kAccountStorageUsernameEntry:
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
    case PopupItemId::kTitle:
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kWebauthnCredential:
    case PopupItemId::kWebauthnSignInWithAnotherDevice:
      return false;
  }
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
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kAndroidWebView:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed:
      // On Android, no popup exists. Instead, the keyboard accessory is used.
#if BUILDFLAG(IS_ANDROID)
      return AutofillTriggerSource::kKeyboardAccessory;
#else
      return AutofillTriggerSource::kPopup;
#endif  // BUILDFLAG(IS_ANDROID)
    case AutofillSuggestionTriggerSource::kManualFallbackAddress:
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

void AutofillExternalDelegate::OnQuery(const FormData& form,
                                       const FormFieldData& field,
                                       const gfx::RectF& element_bounds) {
  query_form_ = form;
  query_field_ = field;
  element_bounds_ = element_bounds;
  should_show_scan_credit_card_ =
      manager_->ShouldShowScanCreditCard(query_form_, query_field_);
  popup_type_ = manager_->GetPopupType(query_form_, query_field_);
  should_show_cards_from_account_option_ =
      manager_->ShouldShowCardsFromAccountOption(query_form_, query_field_);
}

AutofillField* AutofillExternalDelegate::GetQueriedAutofillField() const {
  return manager_->GetAutofillField(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    bool is_all_server_suggestions) {
  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_autofill_suggestions_ = base::ranges::any_of(
      input_suggestions, IsFirstLayerFormFillingSuggestionId,
      &Suggestion::popup_item_id);

  if (field_id != query_field_.global_id()) {
    return;
  }
  if (trigger_source ==
          AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed &&
      !has_autofill_suggestions_) {
    // User changed or delete the only Autofill profile shown in the popup,
    // avoid showing any other suggestions in this case.
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

  if (should_show_scan_credit_card_) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD));
    scan_credit_card.popup_item_id = PopupItemId::kScanCreditCard;
    scan_credit_card.icon = "scanCreditCardIcon";
    suggestions.push_back(scan_credit_card);
  }

  if (should_show_cards_from_account_option_) {
    suggestions.emplace_back(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS));
    suggestions.back().popup_item_id = PopupItemId::kShowAccountCards;
    suggestions.back().icon = "google";
  }

  if (has_autofill_suggestions_)
    ApplyAutofillOptions(&suggestions, is_all_server_suggestions);

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&suggestions);

  if (suggestions.empty()) {
    OnAutofillAvailabilityEvent(mojom::AutofillState::kNoSuggestions);
    // No suggestions, any popup currently showing is obsolete.
    manager_->client().HideAutofillPopup(PopupHidingReason::kNoSuggestions);
    return;
  }

  // Send to display.
  if (query_field_.is_focusable && manager_->driver().CanShowAutofillUi()) {
    AutofillClient::PopupOpenArgs open_args(element_bounds_,
                                            query_field_.text_direction,
                                            suggestions, trigger_source);
    manager_->client().ShowAutofillPopup(open_args, GetWeakPtr());
  }
}

absl::optional<ServerFieldTypeSet>
AutofillExternalDelegate::GetLastFieldTypesToFillForSection(
    const Section& section) const {
  if (auto it =
          last_field_types_to_fill_for_address_form_section_.find(section);
      it != last_field_types_to_fill_for_address_form_section_.end()) {
    return it->second;
  }
  return absl::nullopt;
}

bool AutofillExternalDelegate::HasActiveScreenReader() const {
  // Note: This always returns false if ChromeVox is in use because
  // AXPlatformNodes are not used on the ChromeOS platform.
  return ui::AXPlatformNode::GetAccessibilityMode().has_mode(
      ui::AXMode::kScreenReader);
}

void AutofillExternalDelegate::OnAutofillAvailabilityEvent(
    const mojom::AutofillState state) {
  // Availability of suggestions should be communicated to Blink because
  // accessibility objects live in both the renderer and browser processes.
  manager_->driver().RendererShouldSetSuggestionAvailability(
      query_field_.global_id(), state);
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    std::vector<SelectOption> datalist) {
  datalist_ = std::move(datalist);
  manager_->client().UpdateAutofillPopupDataListValues(datalist_);
}

void AutofillExternalDelegate::OnPopupShown() {
  // Popups are expected to be Autofill or Autocomplete.
  DCHECK_NE(GetPopupType(), PopupType::kPasswords);

  OnAutofillAvailabilityEvent(
      has_autofill_suggestions_ ? mojom::AutofillState::kAutofillAvailable
                                : mojom::AutofillState::kAutocompleteAvailable);
  manager_->DidShowSuggestions(has_autofill_suggestions_, query_form_,
                               query_field_);

  if (should_show_scan_credit_card_) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        AutofillMetrics::SCAN_CARD_ITEM_SHOWN);
  }
}

void AutofillExternalDelegate::OnPopupHidden() {
  manager_->OnPopupHidden();
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const Suggestion& suggestion,
    AutofillSuggestionTriggerSource trigger_source) {
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
               TriggerSourceFromSuggestionTriggerSource(trigger_source)});
      break;
    case PopupItemId::kFillFullAddress:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill = GetAddressFieldsForGroupFilling()});
      break;
    case PopupItemId::kFillFullName:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill =
               GetServerFieldTypesOfGroup(FieldTypeGroup::kName)});
      break;
    case PopupItemId::kFillFullPhoneNumber:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill =
               GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone)});
      break;
    case PopupItemId::kFillFullEmail:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/true,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill =
               GetServerFieldTypesOfGroup(FieldTypeGroup::kEmail)});
      break;
    case PopupItemId::kAutocompleteEntry:
    case PopupItemId::kIbanEntry:
    case PopupItemId::kMerchantPromoCodeEntry:
    case PopupItemId::kFieldByFieldFilling:
    case PopupItemId::kFillExistingPlusAddress:
      manager_->driver().ApplyFieldAction(mojom::ActionPersistence::kPreview,
                                          mojom::TextReplacement::kReplaceAll,
                                          query_field_.global_id(),
                                          suggestion.main_text.value);
      break;
    case PopupItemId::kVirtualCreditCardEntry:
      manager_->FillOrPreviewVirtualCardInformation(
          mojom::ActionPersistence::kPreview, backend_id.value(), query_form_,
          query_field_,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source)});
      break;
    default:
      break;
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(
    const Suggestion& suggestion,
    int position,
    AutofillSuggestionTriggerSource trigger_source) {
  switch (suggestion.popup_item_id) {
    case PopupItemId::kAutofillOptions:
      // User selected 'Autofill Options'.
      autofill_metrics::LogAutofillSelectedManageEntry(popup_type_);
      manager_->ShowAutofillSettings(popup_type_);
      break;
    case PopupItemId::kEditAddressProfile: {
      ShowEditAddressProfileDialog(
          suggestion.GetPayload<Suggestion::BackendId>().value());
      break;
    }
    case PopupItemId::kDeleteAddressProfile:
      ShowDeleteAddressProfileDialog(
          suggestion.GetPayload<Suggestion::BackendId>().value());
      break;
    case PopupItemId::kClearForm:
      // This serves as a clear form or undo autofill suggestion, depending on
      // the state of the feature `kAutofillUndo`.
      if (base::FeatureList::IsEnabled(features::kAutofillUndo)) {
        AutofillMetrics::LogAutofillUndo();
        manager_->UndoAutofill(mojom::ActionPersistence::kFill, query_form_,
                               query_field_);
      } else {
        // User selected 'Clear form'.
        AutofillMetrics::LogAutofillFormCleared();
        manager_->driver().RendererShouldClearFilledSection();
      }
      break;
    case PopupItemId::kPasswordEntry:
    case PopupItemId::kUsernameEntry:
    case PopupItemId::kAccountStoragePasswordEntry:
    case PopupItemId::kAccountStorageUsernameEntry:
      NOTREACHED();  // Should be handled elsewhere.
      break;
    case PopupItemId::kDatalistEntry:
      manager_->driver().RendererShouldAcceptDataListSuggestion(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case PopupItemId::kFieldByFieldFilling:
      if (AutofillField* autofill_trigger_field = GetQueriedAutofillField()) {
        // We target only the triggering field type in the
        // PopupItemId::kFieldByFieldFilling case.
        last_field_types_to_fill_for_address_form_section_
            [autofill_trigger_field->section] = {
                autofill_trigger_field->Type().GetStorableType()};
        const bool had_value_before_filling =
            !autofill_trigger_field->value.empty();
        manager_->driver().ApplyFieldAction(mojom::ActionPersistence::kFill,
                                            mojom::TextReplacement::kReplaceAll,
                                            query_field_.global_id(),
                                            suggestion.main_text.value);
        autofill_trigger_field->is_autofilled = true;
        autofill_trigger_field->AppendLogEventIfNotRepeated(FillFieldLogEvent{
            .fill_event_id = GetNextFillEventId(),
            .had_value_before_filling =
                ToOptionalBoolean(had_value_before_filling),
            .autofill_skipped_status = FieldFillingSkipReason::kNotSkipped,
            .was_autofilled = ToOptionalBoolean(true),
            .had_value_after_filling = ToOptionalBoolean(true),
            .filling_method = AutofillFillingMethod::kFieldByFieldFilling});
      }
      break;
    case PopupItemId::kIbanEntry:
      // User selected an IBAN suggestion, and we should fill the unmasked IBAN
      // value.
      manager_->driver().ApplyFieldAction(
          mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
          query_field_.global_id(),
          suggestion.GetPayload<Suggestion::ValueToFill>().value());
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.popup_item_id,
                                                query_form_, query_field_);
      break;
    case PopupItemId::kFillFullAddress:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill = GetAddressFieldsForGroupFilling()});
      break;
    case PopupItemId::kFillFullName:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill =
               GetServerFieldTypesOfGroup(FieldTypeGroup::kName)});
      break;
    case PopupItemId::kFillFullPhoneNumber:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill =
               GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone)});
      break;
    case PopupItemId::kFillFullEmail:
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), /*is_preview=*/false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source),
           .field_types_to_fill =
               GetServerFieldTypesOfGroup(FieldTypeGroup::kEmail)});
      break;
    case PopupItemId::kAutocompleteEntry:
      AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(position);
      ABSL_FALLTHROUGH_INTENDED;
    case PopupItemId::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->driver().ApplyFieldAction(
          mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
          query_field_.global_id(), suggestion.main_text.value);
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
      manager_->OnUserAcceptedCardsFromAccountOption();
      break;
    case PopupItemId::kVirtualCreditCardEntry:
      // There can be multiple virtual credit cards that all rely on
      // PopupItemId::kVirtualCreditCardEntry as a `popup_item_id`. In this
      // case, the payload contains the backend id, which is a GUID that
      // identifies the actually chosen credit card.
      manager_->FillOrPreviewVirtualCardInformation(
          mojom::ActionPersistence::kFill,
          suggestion.GetPayload<Suggestion::BackendId>().value(), query_form_,
          query_field_, {.trigger_source = AutofillTriggerSource::kPopup});
      break;
    case PopupItemId::kSeePromoCodeDetails:
      manager_->OnSeePromoCodeOfferDetailsSelected(
          suggestion.GetPayload<GURL>(), suggestion.main_text.value,
          suggestion.popup_item_id, query_form_, query_field_);
      break;
    case PopupItemId::kFillExistingPlusAddress:
      plus_addresses::PlusAddressMetrics::RecordAutofillSuggestionEvent(
          plus_addresses::PlusAddressMetrics::
              PlusAddressAutofillSuggestionEvent::kExistingPlusAddressChosen);
      manager_->driver().ApplyFieldAction(
          mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case PopupItemId::kCreateNewPlusAddress: {
      plus_addresses::PlusAddressMetrics::RecordAutofillSuggestionEvent(
          plus_addresses::PlusAddressMetrics::
              PlusAddressAutofillSuggestionEvent::kCreateNewPlusAddressChosen);
      plus_addresses::PlusAddressCallback callback = base::BindOnce(
          [](base::WeakPtr<AutofillManager> manager, FieldGlobalId field,
             const std::string& plus_address) {
            if (manager) {
              manager->driver().ApplyFieldAction(
                  mojom::ActionPersistence::kFill,
                  mojom::TextReplacement::kReplaceAll, field,
                  base::UTF8ToUTF16(plus_address));
            }
          },
          manager_->GetWeakPtr(), query_field_.global_id());
      manager_->client().OfferPlusAddressCreation(
          manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
          std::move(callback));
      break;
    }
    case PopupItemId::kCompose:
      if (AutofillComposeDelegate* delegate =
              manager_->client().GetComposeDelegate()) {
        AutofillComposeDelegate::ComposeCallback callback = base::BindOnce(
            [](base::WeakPtr<AutofillManager> manager, FieldGlobalId field,
               const std::u16string& text) {
              if (manager) {
                manager->driver().ApplyFieldAction(
                    mojom::ActionPersistence::kFill,
                    mojom::TextReplacement::kReplaceSelection, field, text);
              }
            },
            manager_->GetWeakPtr(), query_field_.global_id());
        delegate->OpenCompose(
            AutofillComposeDelegate::UiEntryPoint::kAutofillPopup, query_field_,
            manager_->client().GetPopupScreenLocation(), std::move(callback));
      }
      break;
    default:
      if (suggestion.popup_item_id == PopupItemId::kAddressEntry ||
          suggestion.popup_item_id == PopupItemId::kCreditCardEntry ||
          suggestion.popup_item_id ==
              PopupItemId::kFillEverythingFromAddressProfile) {
        autofill_metrics::LogAutofillSuggestionAcceptedIndex(
            position, popup_type_, manager_->client().IsOffTheRecord());
      }
      if (suggestion.popup_item_id == PopupItemId::kAddressEntry &&
          manager_->WasSuggestionPreviouslyHidden(
              query_form_, query_field_,
              suggestion.GetPayload<Suggestion::BackendId>(), trigger_source)) {
        autofill_metrics::LogUserAcceptedPreviouslyHiddenProfileSuggestion();
      }
      FillAutofillFormData(
          suggestion.popup_item_id,
          suggestion.GetPayload<Suggestion::BackendId>(), false,
          {.trigger_source =
               TriggerSourceFromSuggestionTriggerSource(trigger_source)});
      break;
  }

  if (should_show_scan_credit_card_) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        suggestion.popup_item_id == PopupItemId::kScanCreditCard
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }

  if (suggestion.popup_item_id == PopupItemId::kShowAccountCards) {
    should_show_cards_from_account_option_ = false;
    manager_->RefetchCardsAndUpdatePopup(query_form_, query_field_);
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

bool AutofillExternalDelegate::GetDeletionConfirmationText(
    const std::u16string& value,
    PopupItemId popup_item_id,
    Suggestion::BackendId backend_id,
    std::u16string* title,
    std::u16string* body) {
  return manager_->GetDeletionConfirmationText(value, popup_item_id, backend_id,
                                               title, body);
}

bool AutofillExternalDelegate::RemoveSuggestion(
    const std::u16string& value,
    PopupItemId popup_item_id,
    Suggestion::BackendId backend_id) {
  if (popup_item_id == PopupItemId::kAddressEntry ||
      popup_item_id == PopupItemId::kCreditCardEntry) {
    return manager_->RemoveAutofillProfileOrCreditCard(backend_id);
  }

  if (popup_item_id == PopupItemId::kAutocompleteEntry) {
    manager_->RemoveCurrentSingleFieldSuggestion(query_field_.name, value,
                                                 popup_item_id);
    return true;
  }

  return false;
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client().HideAutofillPopup(PopupHidingReason::kEndEditing);
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  manager_->driver().RendererShouldClearPreviewedForm();
}

PopupType AutofillExternalDelegate::GetPopupType() const {
  return popup_type_;
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
  manager_->driver().RendererShouldTriggerSuggestions(
      query_field_.global_id(),
      AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed);
}

void AutofillExternalDelegate::OnDeleteDialogClosed(const std::string& guid,
                                                    bool user_accepted_delete) {
  autofill_metrics::LogDeleteAddressProfileDialogClosed(user_accepted_delete);
  if (user_accepted_delete) {
    PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
    if (!pdm_observation_.IsObserving()) {
      pdm_observation_.Observe(pdm);
    }
    pdm->RemoveByGUID(guid);
    return;
  }
  manager_->driver().RendererShouldTriggerSuggestions(
      query_field_.global_id(),
      AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed);
}

void AutofillExternalDelegate::OnPersonalDataFinishedProfileTasks() {
  pdm_observation_.Reset();
  manager_->driver().RendererShouldTriggerSuggestions(
      query_field_.global_id(),
      AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed);
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillCreditCardForm(query_form_, query_field_, card,
                               std::u16string(),
                               {.trigger_source = trigger_source});
}

void AutofillExternalDelegate::FillAutofillFormData(
    PopupItemId popup_item_id,
    Suggestion::BackendId backend_id,
    bool is_preview,
    const AutofillTriggerDetails& trigger_details) {
  // If the selected element is a warning we don't want to do anything.
  if (IsAutofillWarningEntry(popup_item_id)) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kAutofillGranularFillingAvailable)) {
    // Only address suggestions store the last field types to
    // fill. This is because this is the only use case where filling
    // granularies need to be persisted.
    static constexpr auto kAutofillAddressSuggestions =
        base::MakeFixedFlatSet<PopupItemId>(
            {PopupItemId::kAddressEntry, PopupItemId::kFillFullAddress,
             PopupItemId::kFillFullPhoneNumber, PopupItemId::kFillFullName,
             PopupItemId::kFillEverythingFromAddressProfile});
    AutofillField* autofill_trigger_field = GetQueriedAutofillField();
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

  DCHECK(manager_->driver().RendererIsAvailable());
  PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
  if (const CreditCard* credit_card =
          pdm->GetCreditCardByGUID(backend_id.value())) {
    manager_->FillOrPreviewCreditCardForm(action_persistence, query_form_,
                                          query_field_, credit_card,
                                          trigger_details);
  } else if (const AutofillProfile* profile =
                 pdm->GetProfileByGUID(backend_id.value())) {
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

void AutofillExternalDelegate::ApplyAutofillOptions(
    std::vector<Suggestion>* suggestions,
    bool is_all_server_suggestions) {
#if !BUILDFLAG(IS_ANDROID)
  // Add a separator before the Autofill options unless there are no suggestions
  // yet.
  // TODO(crbug.com/1274134): Clean up once improvements are launched.
  if (!suggestions->empty()) {
    suggestions->push_back(Suggestion(PopupItemId::kSeparator));
  }
#endif

  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (query_field_.is_autofilled) {
    std::u16string value =
        base::FeatureList::IsEnabled(features::kAutofillUndo)
            ? l10n_util::GetStringUTF16(IDS_AUTOFILL_UNDO_MENU_ITEM)
            : l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
#if BUILDFLAG(IS_ANDROID)
    if (IsKeyboardAccessoryEnabled())
      value = base::i18n::ToUpper(value);
#endif

    suggestions->emplace_back(value);
    suggestions->back().popup_item_id = PopupItemId::kClearForm;
    suggestions->back().icon =
        base::FeatureList::IsEnabled(features::kAutofillUndo) ? "undoIcon"
                                                              : "clearIcon";
    suggestions->back().acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  }

  // Append the 'Autofill settings' menu item, or the menu item specified in the
  // popup layout experiment.
  suggestions->emplace_back(GetSettingsSuggestionValue());
  suggestions->back().popup_item_id = PopupItemId::kAutofillOptions;
  suggestions->back().icon = "settingsIcon";

  // On Android and Desktop, Google Pay branding is shown along with Settings.
  // So Google Pay Icon is just attached to an existing menu item.
  if (is_all_server_suggestions) {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    suggestions->back().icon = "googlePay";
#else
    suggestions->back().trailing_icon =
        ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
            ? "googlePayDark"
            : "googlePay";
#endif
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
  base::EraseIf(*suggestions, [&datalist_values](const Suggestion& suggestion) {
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

std::u16string AutofillExternalDelegate::GetSettingsSuggestionValue() const {
  switch (GetPopupType()) {
    case PopupType::kAddresses:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES);

    case PopupType::kCreditCards:
    case PopupType::kIbans:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS);

    case PopupType::kPersonalInformation:
    case PopupType::kUnspecified:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE);

    case PopupType::kPasswords:
      NOTREACHED();
      return std::u16string();
  }
}

}  // namespace autofill
