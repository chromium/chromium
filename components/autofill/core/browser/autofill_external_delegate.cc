// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include <utility>

#include "autofill_trigger_details.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
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
    case AutofillSuggestionTriggerSource::kTextFieldDidChange:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kShowCardsFromAccount:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kAndroidWebView:
    case AutofillSuggestionTriggerSource::kiOS:
      // On Android, no popup exists. Instead, the keyboard accessory is used.
#if BUILDFLAG(IS_ANDROID)
      return AutofillTriggerSource::kKeyboardAccessory;
#else
      return AutofillTriggerSource::kPopup;
#endif  // BUILDFLAG(IS_ANDROID)
    case AutofillSuggestionTriggerSource::
        kManualFallbackForAutocompleteUnrecognized:
      // Manual fallbacks are both a suggestion trigger source (e.g. through the
      // context menu) and a trigger source (by selecting a suggestion generated
      // through the context menu).
      return AutofillTriggerSource::kManualFallbackForAutocompleteUnrecognized;
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

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions,
    AutofillSuggestionTriggerSource trigger_source,
    bool is_all_server_suggestions) {
  if (field_id != query_field_.global_id())
    return;
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

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_autofill_suggestions_ = false;
  for (auto& suggestion : suggestions) {
    // Virtual cards can appear on their own when filling the CVC for a card
    // that a merchant has saved. This indicates there could be Autofill
    // suggestions related to standalone CVC fields.
    if (suggestion.popup_item_id == PopupItemId::kAddressEntry ||
        suggestion.popup_item_id == PopupItemId::kCreditCardEntry ||
        suggestion.popup_item_id == PopupItemId::kVirtualCreditCardEntry) {
      has_autofill_suggestions_ = true;
      break;
    }
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
    const std::vector<std::u16string>& data_list_values,
    const std::vector<std::u16string>& data_list_labels) {
  data_list_values_ = data_list_values;
  data_list_labels_ = data_list_labels;

  manager_->client().UpdateAutofillPopupDataListValues(data_list_values,
                                                       data_list_labels);
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
        manager_->UndoAutofill(mojom::AutofillActionPersistence::kPreview,
                               query_form_, query_field_);
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
    case PopupItemId::kAutocompleteEntry:
    case PopupItemId::kIbanEntry:
    case PopupItemId::kMerchantPromoCodeEntry:
    case PopupItemId::kFieldByFieldFilling:
    case PopupItemId::kFillExistingPlusAddress:
      manager_->driver().RendererShouldPreviewFieldWithValue(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case PopupItemId::kVirtualCreditCardEntry:
      manager_->FillOrPreviewVirtualCardInformation(
          mojom::AutofillActionPersistence::kPreview, backend_id.value(),
          query_form_, query_field_,
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
    case PopupItemId::kEditAddressProfile:
      // TODO(crbug.com/1459990): call the manager to display the edit address
      // profile dialog.
      break;
    case PopupItemId::kDeleteAddressProfile:
      // TODO(crbug.com/1459990): call the manager to display the delete address
      // profile dialog.
      break;
    case PopupItemId::kClearForm:
      // This serves as a clear form or undo autofill suggestion, depending on
      // the state of the feature `kAutofillUndo`.
      if (base::FeatureList::IsEnabled(features::kAutofillUndo)) {
        AutofillMetrics::LogAutofillUndo();
        manager_->UndoAutofill(mojom::AutofillActionPersistence::kFill,
                               query_form_, query_field_);
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
      manager_->driver().RendererShouldFillFieldWithValue(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case PopupItemId::kIbanEntry:
      // User selected an IBAN suggestion, and we should fill the unmasked IBAN
      // value.
      manager_->driver().RendererShouldFillFieldWithValue(
          query_field_.global_id(),
          suggestion.GetPayload<Suggestion::ValueToFill>().value());
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.popup_item_id,
                                                query_form_, query_field_);
      break;
    case PopupItemId::kAutocompleteEntry:
      AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(position);
      ABSL_FALLTHROUGH_INTENDED;
    case PopupItemId::kMerchantPromoCodeEntry:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      manager_->driver().RendererShouldFillFieldWithValue(
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
          mojom::AutofillActionPersistence::kFill,
          suggestion.GetPayload<Suggestion::BackendId>().value(), query_form_,
          query_field_, {.trigger_source = AutofillTriggerSource::kPopup});
      break;
    case PopupItemId::kSeePromoCodeDetails:
      manager_->OnSeePromoCodeOfferDetailsSelected(
          suggestion.GetPayload<GURL>(), suggestion.main_text.value,
          suggestion.popup_item_id, query_form_, query_field_);
      break;
    case PopupItemId::kFillExistingPlusAddress:
      manager_->driver().RendererShouldFillFieldWithValue(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case PopupItemId::kCreateNewPlusAddress: {
      manager_->client().OfferPlusAddressCreation(
          manager_->client().GetLastCommittedPrimaryMainFrameOrigin(),
          base::BindOnce(&AutofillExternalDelegate::OnPlusAddressCreated,
                         GetWeakPtr()));
      break;
    }
    default:
      if (suggestion.popup_item_id == PopupItemId::kAddressEntry ||
          suggestion.popup_item_id == PopupItemId::kCreditCardEntry ||
          suggestion.popup_item_id ==
              PopupItemId::kFillEverythingFromAddressProfile) {
        autofill_metrics::LogAutofillSuggestionAcceptedIndex(
            position, popup_type_, manager_->client().IsOffTheRecord());
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

void AutofillExternalDelegate::Reset() {
  // We should not affect UI on the active page due to a prerendered page.
  if (!manager_->driver().IsPrerendering()) {
    manager_->client().HideAutofillPopup(PopupHidingReason::kNavigation);
  }
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillCreditCardFormImpl(query_form_, query_field_, card,
                                   std::u16string(),
                                   {.trigger_source = trigger_source});
}

void AutofillExternalDelegate::OnPlusAddressCreated(
    const std::string& plus_address) {
  manager_->driver().RendererShouldFillFieldWithValue(
      query_field_.global_id(), base::UTF8ToUTF16(plus_address));
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

  mojom::AutofillActionPersistence action_persistence =
      is_preview ? mojom::AutofillActionPersistence::kPreview
                 : mojom::AutofillActionPersistence::kFill;

  DCHECK(manager_->driver().RendererIsAvailable());
  // Fill the values for the whole form.
  manager_->FillOrPreviewForm(action_persistence, query_form_, query_field_,
                              backend_id, trigger_details);
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
  if (data_list_values_.empty())
    return;

  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  std::set<std::u16string> data_list_set(data_list_values_.begin(),
                                         data_list_values_.end());
  base::EraseIf(*suggestions, [&data_list_set](const Suggestion& suggestion) {
    return suggestion.popup_item_id == PopupItemId::kAutocompleteEntry &&
           base::Contains(data_list_set, suggestion.main_text.value);
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
  suggestions->insert(suggestions->begin(), data_list_values_.size(),
                      Suggestion());
  for (size_t i = 0; i < data_list_values_.size(); i++) {
    (*suggestions)[i].main_text = Suggestion::Text(
        data_list_values_[i], Suggestion::Text::IsPrimary(true));
    (*suggestions)[i].labels = {{Suggestion::Text(data_list_labels_[i])}};
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
