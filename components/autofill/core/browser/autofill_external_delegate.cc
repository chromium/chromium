// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include <utility>

#include "base/check.h"
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
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
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
bool IsAutofillWarningEntry(int frontend_id) {
  return frontend_id ==
             POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE ||
         frontend_id == POPUP_ITEM_ID_MIXED_FORM_MESSAGE;
}

}  // namespace

AutofillExternalDelegate::AutofillExternalDelegate(
    BrowserAutofillManager* manager,
    AutofillDriver* driver)
    : manager_(manager), driver_(driver) {
  DCHECK(manager);
}

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
  should_show_cc_signin_promo_ =
      manager_->ShouldShowCreditCardSigninPromo(query_form_, query_field_);
  should_show_cards_from_account_option_ =
      manager_->ShouldShowCardsFromAccountOption(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    FieldGlobalId field_id,
    const std::vector<Suggestion>& input_suggestions,
    AutoselectFirstSuggestion autoselect_first_suggestion,
    bool is_all_server_suggestions) {
  if (field_id != query_field_.global_id())
    return;
#if BUILDFLAG(IS_IOS)
  if (!manager_->client()->IsLastQueriedField(field_id))
    return;
#endif

  std::vector<Suggestion> suggestions(input_suggestions);

  // Hide warnings as appropriate.
  PossiblyRemoveAutofillWarnings(&suggestions);

  if (should_show_scan_credit_card_) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD));
    scan_credit_card.frontend_id = POPUP_ITEM_ID_SCAN_CREDIT_CARD;
    scan_credit_card.icon = "scanCreditCardIcon";
    suggestions.push_back(scan_credit_card);
  }

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_autofill_suggestions_ = false;
  for (auto& suggestion : suggestions) {
    if (suggestion.frontend_id > 0) {
      has_autofill_suggestions_ = true;
      break;
    }
  }

  if (should_show_cards_from_account_option_) {
    suggestions.emplace_back(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS));
    suggestions.back().frontend_id = POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS;
    suggestions.back().icon = "google";
  }

  if (has_autofill_suggestions_)
    ApplyAutofillOptions(&suggestions, is_all_server_suggestions);

  // Append the credit card signin promo, if appropriate (there are no other
  // suggestions).
  if (suggestions.empty() && should_show_cc_signin_promo_) {
    Suggestion signin_promo_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CREDIT_CARD_SIGNIN_PROMO));
    signin_promo_suggestion.frontend_id =
        POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO;
    suggestions.push_back(signin_promo_suggestion);
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
  }

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&suggestions);

  if (suggestions.empty()) {
    OnAutofillAvailabilityEvent(mojom::AutofillState::kNoSuggestions);
    // No suggestions, any popup currently showing is obsolete.
    manager_->client()->HideAutofillPopup(PopupHidingReason::kNoSuggestions);
    return;
  }

  // Send to display.
  if (query_field_.is_focusable && driver_->CanShowAutofillUi()) {
    AutofillClient::PopupOpenArgs open_args(
        element_bounds_, query_field_.text_direction, suggestions,
        AutoselectFirstSuggestion(autoselect_first_suggestion));
    manager_->client()->ShowAutofillPopup(open_args, GetWeakPtr());
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
  driver_->RendererShouldSetSuggestionAvailability(query_field_.global_id(),
                                                   state);
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    const std::vector<std::u16string>& data_list_values,
    const std::vector<std::u16string>& data_list_labels) {
  data_list_values_ = data_list_values;
  data_list_labels_ = data_list_labels;

  manager_->client()->UpdateAutofillPopupDataListValues(data_list_values,
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
  driver_->PopupHidden();
}

void AutofillExternalDelegate::OnPopupSuppressed() {
  manager_->DidSuppressPopup(query_form_, query_field_);
}

void AutofillExternalDelegate::DidSelectSuggestion(
    const std::u16string& value,
    int frontend_id,
    const Suggestion::BackendId& backend_id) {
  ClearPreviewedForm();

  // Only preview the data if it is a profile or a virtual card.
  if (frontend_id > 0) {
    FillAutofillFormData(frontend_id, true,
                         AutofillTriggerSource::kKeyboardAccessory);
  } else if (frontend_id == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY ||
             frontend_id == POPUP_ITEM_ID_IBAN_ENTRY ||
             frontend_id == POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY) {
    driver_->RendererShouldPreviewFieldWithValue(query_field_.global_id(),
                                                 value);
  } else if (frontend_id == POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY) {
    manager_->FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kPreview, backend_id.value(),
        query_form_, query_field_, AutofillTriggerSource::kKeyboardAccessory);
  }
}

void AutofillExternalDelegate::DidAcceptSuggestion(const Suggestion& suggestion,
                                                   int position) {
  switch (suggestion.frontend_id) {
    case POPUP_ITEM_ID_AUTOFILL_OPTIONS:
      // User selected 'Autofill Options'.
      autofill_metrics::LogAutofillSelectedManageEntry(popup_type_);
      manager_->ShowAutofillSettings(popup_type_);
      break;
    case POPUP_ITEM_ID_CLEAR_FORM:
      // User selected 'Clear form'.
      AutofillMetrics::LogAutofillFormCleared();
      driver_->RendererShouldClearFilledSection();
      break;
    case POPUP_ITEM_ID_PASSWORD_ENTRY:
    case POPUP_ITEM_ID_USERNAME_ENTRY:
    case POPUP_ITEM_ID_ACCOUNT_STORAGE_PASSWORD_ENTRY:
    case POPUP_ITEM_ID_ACCOUNT_STORAGE_USERNAME_ENTRY:
      NOTREACHED();  // Should be handled elsewhere.
      break;
    case POPUP_ITEM_ID_DATALIST_ENTRY:
      driver_->RendererShouldAcceptDataListSuggestion(
          query_field_.global_id(), suggestion.main_text.value);
      break;
    case POPUP_ITEM_ID_IBAN_ENTRY:
      // User selected an IBAN suggestion, and we should fill the unmasked IBAN
      // value.
      driver_->RendererShouldFillFieldWithValue(
          query_field_.global_id(),
          suggestion.GetPayload<Suggestion::ValueToFill>().value());
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.frontend_id,
                                                query_form_, query_field_);
      break;
    case POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY:
      AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(position);
      ABSL_FALLTHROUGH_INTENDED;
    case POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY:
      // User selected an Autocomplete or Merchant Promo Code field, so we fill
      // directly.
      driver_->RendererShouldFillFieldWithValue(query_field_.global_id(),
                                                suggestion.main_text.value);
      manager_->OnSingleFieldSuggestionSelected(suggestion.main_text.value,
                                                suggestion.frontend_id,
                                                query_form_, query_field_);
      break;
    case POPUP_ITEM_ID_SCAN_CREDIT_CARD:
      manager_->client()->ScanCreditCard(
          base::BindOnce(&AutofillExternalDelegate::OnCreditCardScanned,
                         GetWeakPtr(), AutofillTriggerSource::kPopup));
      break;
    case POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO:
      manager_->client()->ExecuteCommand(suggestion.frontend_id);
      break;
    case POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS:
      manager_->OnUserAcceptedCardsFromAccountOption();
      break;
    case POPUP_ITEM_ID_USE_VIRTUAL_CARD:
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
      manager_->FetchVirtualCardCandidates();
#else
      NOTREACHED();
#endif
      break;
    case POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY:
      // There can be multiple virtual credit cards that all rely on
      // POPUP_ITEM_ID_VIRTUAL_CREDIT_CARD_ENTRY as a frontend_id. In this case,
      // the payload contains the backend id, which is a GUID that identifies
      // the actually chosen credit card.
      manager_->FillOrPreviewVirtualCardInformation(
          mojom::RendererFormDataAction::kFill,
          suggestion.GetPayload<Suggestion::BackendId>().value(), query_form_,
          query_field_, AutofillTriggerSource::kPopup);
      break;
    case POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS:
      manager_->OnSeePromoCodeOfferDetailsSelected(
          suggestion.GetPayload<GURL>(), suggestion.main_text.value,
          suggestion.frontend_id, query_form_, query_field_);
      break;
    default:
      if (suggestion.frontend_id > 0) {  // Denotes an Autofill suggestion.
        autofill_metrics::LogAutofillSuggestionAcceptedIndex(
            position, popup_type_, manager_->client()->IsOffTheRecord());
      }
      FillAutofillFormData(suggestion.frontend_id, false,
                           AutofillTriggerSource::kPopup);
      break;
  }

  if (should_show_scan_credit_card_) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        suggestion.frontend_id == POPUP_ITEM_ID_SCAN_CREDIT_CARD
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }

  if (suggestion.frontend_id == POPUP_ITEM_ID_SHOW_ACCOUNT_CARDS) {
    should_show_cards_from_account_option_ = false;
    manager_->RefetchCardsAndUpdatePopup(query_form_, query_field_);
  } else {
    manager_->client()->HideAutofillPopup(PopupHidingReason::kAcceptSuggestion);
  }
}

bool AutofillExternalDelegate::GetDeletionConfirmationText(
    const std::u16string& value,
    int frontend_id,
    std::u16string* title,
    std::u16string* body) {
  return manager_->GetDeletionConfirmationText(value, frontend_id, title, body);
}

bool AutofillExternalDelegate::RemoveSuggestion(const std::u16string& value,
                                                int frontend_id) {
  if (frontend_id > 0)
    return manager_->RemoveAutofillProfileOrCreditCard(frontend_id);

  if (frontend_id == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    manager_->RemoveCurrentSingleFieldSuggestion(query_field_.name, value,
                                                 frontend_id);
    return true;
  }

  return false;
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client()->HideAutofillPopup(PopupHidingReason::kEndEditing);
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  driver_->RendererShouldClearPreviewedForm();
}

PopupType AutofillExternalDelegate::GetPopupType() const {
  return popup_type_;
}

absl::variant<AutofillDriver*, password_manager::PasswordManagerDriver*>
AutofillExternalDelegate::GetDriver() {
  return driver_.get();
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
  if (!manager_->driver()->IsPrerendering())
    manager_->client()->HideAutofillPopup(PopupHidingReason::kNavigation);
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::OnCreditCardScanned(
    const AutofillTriggerSource trigger_source,
    const CreditCard& card) {
  manager_->FillCreditCardFormImpl(query_form_, query_field_, card,
                                   std::u16string(), trigger_source);
}

void AutofillExternalDelegate::FillAutofillFormData(
    int unique_id,
    bool is_preview,
    const AutofillTriggerSource trigger_source) {
  // If the selected element is a warning we don't want to do anything.
  if (IsAutofillWarningEntry(unique_id))
    return;

  mojom::RendererFormDataAction renderer_action =
      is_preview ? mojom::RendererFormDataAction::kPreview
                 : mojom::RendererFormDataAction::kFill;

  DCHECK(driver_->RendererIsAvailable());
  // Fill the values for the whole form.
  manager_->FillOrPreviewForm(renderer_action, query_form_, query_field_,
                              unique_id, trigger_source);
}

void AutofillExternalDelegate::PossiblyRemoveAutofillWarnings(
    std::vector<Suggestion>* suggestions) {
  while (suggestions->size() > 1 &&
         IsAutofillWarningEntry(suggestions->front().frontend_id) &&
         !IsAutofillWarningEntry(suggestions->back().frontend_id)) {
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
    suggestions->push_back(Suggestion(POPUP_ITEM_ID_SEPARATOR));
  }
#endif

  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (query_field_.is_autofilled) {
    std::u16string value =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
#if BUILDFLAG(IS_ANDROID)
    if (IsKeyboardAccessoryEnabled())
      value = base::i18n::ToUpper(value);
#endif

    suggestions->push_back(Suggestion(value));
    suggestions->back().frontend_id = POPUP_ITEM_ID_CLEAR_FORM;
    suggestions->back().icon = "clearIcon";
    suggestions->back().acceptance_a11y_announcement =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_A11Y_ANNOUNCE_CLEARED_FORM);
  }

  // Append the 'Autofill settings' menu item, or the menu item specified in the
  // popup layout experiment.
  suggestions->push_back(Suggestion(GetSettingsSuggestionValue()));
  suggestions->back().frontend_id = POPUP_ITEM_ID_AUTOFILL_OPTIONS;
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
    return suggestion.frontend_id == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY &&
           base::Contains(data_list_set, suggestion.main_text.value);
  });

#if !BUILDFLAG(IS_ANDROID)
  // Insert the separator between the datalist and Autofill/Autocomplete values
  // (if there are any).
  if (!suggestions->empty()) {
    suggestions->insert(suggestions->begin(),
                        Suggestion(POPUP_ITEM_ID_SEPARATOR));
  }
#endif

  // Insert the datalist elements at the beginning.
  suggestions->insert(suggestions->begin(), data_list_values_.size(),
                      Suggestion());
  for (size_t i = 0; i < data_list_values_.size(); i++) {
    (*suggestions)[i].main_text = Suggestion::Text(
        data_list_values_[i], Suggestion::Text::IsPrimary(true));
    (*suggestions)[i].labels = {{Suggestion::Text(data_list_labels_[i])}};
    (*suggestions)[i].frontend_id = POPUP_ITEM_ID_DATALIST_ENTRY;
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
