// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/i18n/case_conversion.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/signin/core/browser/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Returns true if the suggestion entry is an Autofill warning message.
// Warning messages should display on top of suggestion list.
bool IsAutofillWarningEntry(int frontend_id) {
  return frontend_id == POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE;
}

}  // namespace

AutofillExternalDelegate::AutofillExternalDelegate(AutofillManager* manager,
                                                   AutofillDriver* driver)
    : manager_(manager),
      driver_(driver),
      query_id_(0),
      has_autofill_suggestions_(false),
      should_show_scan_credit_card_(false),
      popup_type_(PopupType::kUnspecified),
      should_show_cc_signin_promo_(false),
      weak_ptr_factory_(this) {
  DCHECK(manager);
}

AutofillExternalDelegate::~AutofillExternalDelegate() {
  if (deletion_callback_)
    std::move(deletion_callback_).Run();
}

void AutofillExternalDelegate::OnQuery(int query_id,
                                       const FormData& form,
                                       const FormFieldData& field,
                                       const gfx::RectF& element_bounds) {
  query_form_ = form;
  query_field_ = field;
  query_id_ = query_id;
  element_bounds_ = element_bounds;
  should_show_scan_credit_card_ =
      manager_->ShouldShowScanCreditCard(query_form_, query_field_);
  popup_type_ = manager_->GetPopupType(query_form_, query_field_);
  should_show_cc_signin_promo_ =
      manager_->ShouldShowCreditCardSigninPromo(query_form_, query_field_);
}

void AutofillExternalDelegate::OnSuggestionsReturned(
    int query_id,
    const std::vector<Suggestion>& input_suggestions,
    bool autoselect_first_suggestion,
    bool is_all_server_suggestions) {
  if (query_id != query_id_)
    return;

  std::vector<Suggestion> suggestions(input_suggestions);

  // Hide warnings as appropriate.
  PossiblyRemoveAutofillWarnings(&suggestions);

#if !defined(OS_ANDROID)
  // If there are above the fold suggestions at this point, add a separator to
  // go between the values and menu items. Skip this when using the Native Views
  // implementation, which has its own logic for distinguishing footer rows.
  // TODO(crbug.com/831603): Remove this when the relevant feature is on 100%.
  if (!suggestions.empty() && !features::ShouldUseNativeViews()) {
    suggestions.push_back(Suggestion());
    suggestions.back().frontend_id = POPUP_ITEM_ID_SEPARATOR;
  }
#endif

  if (should_show_scan_credit_card_) {
    Suggestion scan_credit_card(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_SCAN_CREDIT_CARD));
    scan_credit_card.frontend_id = POPUP_ITEM_ID_SCAN_CREDIT_CARD;
    scan_credit_card.icon = base::ASCIIToUTF16("scanCreditCardIcon");
    suggestions.push_back(scan_credit_card);
  }

  // Only include "Autofill Options" special menu item if we have Autofill
  // suggestions.
  has_autofill_suggestions_ = false;
  for (size_t i = 0; i < suggestions.size(); ++i) {
    if (suggestions[i].frontend_id > 0) {
      has_autofill_suggestions_ = true;
      break;
    }
  }

  if (has_autofill_suggestions_)
    ApplyAutofillOptions(&suggestions, is_all_server_suggestions);

  // Append the credit card signin promo, if appropriate (there are no other
  // suggestions).
  if (suggestions.empty() && should_show_cc_signin_promo_) {
// No separator on Android.
#if !defined(OS_ANDROID)
    // If there are autofill suggestions, the "Autofill options" row was added
    // above. Add a separator between it and the signin promo.
    if (has_autofill_suggestions_) {
      suggestions.push_back(Suggestion());
      suggestions.back().frontend_id = POPUP_ITEM_ID_SEPARATOR;
    }
#endif

    Suggestion signin_promo_suggestion(
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CREDIT_CARD_SIGNIN_PROMO));
    signin_promo_suggestion.frontend_id =
        POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO;
    suggestions.push_back(signin_promo_suggestion);
    signin_metrics::RecordSigninImpressionUserActionForAccessPoint(
        signin_metrics::AccessPoint::ACCESS_POINT_AUTOFILL_DROPDOWN);
  }

#if !defined(OS_ANDROID)
  // Remove the separator if there is one, and if it is the last element.
  if (!suggestions.empty() &&
      suggestions.back().frontend_id == POPUP_ITEM_ID_SEPARATOR) {
    suggestions.pop_back();
  }
#endif

  // If anything else is added to modify the values after inserting the data
  // list, AutofillPopupControllerImpl::UpdateDataListValues will need to be
  // updated to match.
  InsertDataListValues(&suggestions);

  if (suggestions.empty()) {
    // No suggestions, any popup currently showing is obsolete.
    manager_->client()->HideAutofillPopup();
    return;
  }

  // Send to display.
  if (query_field_.is_focusable) {
    manager_->client()->ShowAutofillPopup(
        element_bounds_, query_field_.text_direction, suggestions,
        autoselect_first_suggestion, GetWeakPtr());
  }
}

bool AutofillExternalDelegate::HasActiveScreenReader() const {
  return ui::AXPlatformNode::GetAccessibilityMode().has_mode(
      ui::AXMode::kScreenReader);
}

void AutofillExternalDelegate::OnAutofillAvailabilityEvent(
    bool has_suggestions) {
  if (has_suggestions) {
    ui::AXPlatformNode::OnInputSuggestionsAvailable();
  } else {
    ui::AXPlatformNode::OnInputSuggestionsUnavailable();
  }
}

void AutofillExternalDelegate::SetCurrentDataListValues(
    const std::vector<base::string16>& data_list_values,
    const std::vector<base::string16>& data_list_labels) {
  data_list_values_ = data_list_values;
  data_list_labels_ = data_list_labels;

  manager_->client()->UpdateAutofillPopupDataListValues(data_list_values,
                                                        data_list_labels);
}

void AutofillExternalDelegate::OnPopupShown() {
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

void AutofillExternalDelegate::DidSelectSuggestion(
    const base::string16& value,
    int identifier) {
  ClearPreviewedForm();

  // Only preview the data if it is a profile.
  if (identifier > 0)
    FillAutofillFormData(identifier, true);
  else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY)
    driver_->RendererShouldPreviewFieldWithValue(value);
}

void AutofillExternalDelegate::DidAcceptSuggestion(const base::string16& value,
                                                   int identifier,
                                                   int position) {
  if (identifier == POPUP_ITEM_ID_AUTOFILL_OPTIONS) {
    // User selected 'Autofill Options'.
    manager_->ShowAutofillSettings(popup_type_ == PopupType::kCreditCards);
  } else if (identifier == POPUP_ITEM_ID_CLEAR_FORM) {
    // User selected 'Clear form'.
    driver_->RendererShouldClearFilledSection();
  } else if (identifier == POPUP_ITEM_ID_PASSWORD_ENTRY ||
             identifier == POPUP_ITEM_ID_USERNAME_ENTRY) {
    NOTREACHED();  // Should be handled elsewhere.
  } else if (identifier == POPUP_ITEM_ID_DATALIST_ENTRY) {
    driver_->RendererShouldAcceptDataListSuggestion(value);
  } else if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    // User selected an Autocomplete, so we fill directly.
    driver_->RendererShouldFillFieldWithValue(value);
    AutofillMetrics::LogAutocompleteSuggestionAcceptedIndex(position);
  } else if (identifier == POPUP_ITEM_ID_SCAN_CREDIT_CARD) {
    manager_->client()->ScanCreditCard(base::Bind(
        &AutofillExternalDelegate::OnCreditCardScanned, GetWeakPtr()));
  } else if (identifier == POPUP_ITEM_ID_CREDIT_CARD_SIGNIN_PROMO) {
    manager_->client()->ExecuteCommand(identifier);
  } else {
    if (identifier > 0)  // Denotes an Autofill suggestion.
      AutofillMetrics::LogAutofillSuggestionAcceptedIndex(position);

    FillAutofillFormData(identifier, false);
  }

  if (should_show_scan_credit_card_) {
    AutofillMetrics::LogScanCreditCardPromptMetric(
        identifier == POPUP_ITEM_ID_SCAN_CREDIT_CARD
            ? AutofillMetrics::SCAN_CARD_ITEM_SELECTED
            : AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED);
  }

  manager_->client()->HideAutofillPopup();
}

bool AutofillExternalDelegate::GetDeletionConfirmationText(
    const base::string16& value,
    int identifier,
    base::string16* title,
    base::string16* body) {
  return manager_->GetDeletionConfirmationText(value, identifier, title, body);
}

bool AutofillExternalDelegate::RemoveSuggestion(const base::string16& value,
                                                int identifier) {
  if (identifier > 0)
    return manager_->RemoveAutofillProfileOrCreditCard(identifier);

  if (identifier == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY) {
    manager_->RemoveAutocompleteEntry(query_field_.name, value);
    return true;
  }

  return false;
}

void AutofillExternalDelegate::DidEndTextFieldEditing() {
  manager_->client()->HideAutofillPopup();
}

void AutofillExternalDelegate::ClearPreviewedForm() {
  driver_->RendererShouldClearPreviewedForm();
}

PopupType AutofillExternalDelegate::GetPopupType() const {
  return popup_type_;
}

AutofillDriver* AutofillExternalDelegate::GetAutofillDriver() {
  return driver_;
}

void AutofillExternalDelegate::RegisterDeletionCallback(
    base::OnceClosure deletion_callback) {
  deletion_callback_ = std::move(deletion_callback);
}

void AutofillExternalDelegate::Reset() {
  manager_->client()->HideAutofillPopup();
}

base::WeakPtr<AutofillExternalDelegate> AutofillExternalDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillExternalDelegate::OnCreditCardScanned(const CreditCard& card) {
  manager_->FillCreditCardForm(query_id_, query_form_, query_field_, card,
                               base::string16());
}

void AutofillExternalDelegate::FillAutofillFormData(int unique_id,
                                                    bool is_preview) {
  // If the selected element is a warning we don't want to do anything.
  if (IsAutofillWarningEntry(unique_id))
    return;

  AutofillDriver::RendererFormDataAction renderer_action = is_preview ?
      AutofillDriver::FORM_DATA_ACTION_PREVIEW :
      AutofillDriver::FORM_DATA_ACTION_FILL;

  DCHECK(driver_->RendererIsAvailable());
  // Fill the values for the whole form.
  manager_->FillOrPreviewForm(renderer_action,
                              query_id_,
                              query_form_,
                              query_field_,
                              unique_id);
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
  // The form has been auto-filled, so give the user the chance to clear the
  // form.  Append the 'Clear form' menu item.
  if (query_field_.is_autofilled) {
    base::string16 value =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_CLEAR_FORM_MENU_ITEM);
#if defined(OS_ANDROID)
    if (IsKeyboardAccessoryEnabled())
      value = base::i18n::ToUpper(value);
#endif

    suggestions->push_back(Suggestion(value));
    suggestions->back().frontend_id = POPUP_ITEM_ID_CLEAR_FORM;
  }

  // Append the 'Autofill settings' menu item, or the menu item specified in the
  // popup layout experiment. If we do not include |POPUP_ITEM_ID_CLEAR_FORM|,
  // include a hint for keyboard accessory.
  suggestions->push_back(Suggestion(GetSettingsSuggestionValue()));
  suggestions->back().frontend_id = POPUP_ITEM_ID_AUTOFILL_OPTIONS;
  // On Android and Desktop, Google Pay branding is shown along with Settings.
  // So Google Pay Icon is just attached to an existing menu item.
  if (is_all_server_suggestions)
    suggestions->back().icon = base::ASCIIToUTF16("googlePay");

// On iOS, GooglePayIcon comes at the begining and hence prepended to the list.
#if defined(OS_IOS)
  if (base::FeatureList::IsEnabled(
          features::kAutofillDownstreamUseGooglePayBrandingOniOS) &&
      is_all_server_suggestions) {
    Suggestion googlepay_icon;
    googlepay_icon.icon = base::ASCIIToUTF16("googlePay");
    googlepay_icon.frontend_id = POPUP_ITEM_ID_GOOGLE_PAY_BRANDING;
    suggestions->insert(suggestions->begin(), googlepay_icon);
  }
#endif

#if defined(OS_ANDROID)
  if (IsKeyboardAccessoryEnabled()) {
    suggestions->back().icon = base::ASCIIToUTF16("settings");
    if (IsHintEnabledInKeyboardAccessory() && !query_field_.is_autofilled) {
      Suggestion create_icon;
      create_icon.icon = base::ASCIIToUTF16("create");
      create_icon.frontend_id = POPUP_ITEM_ID_CREATE_HINT;
      suggestions->push_back(create_icon);
    }
  }
#endif
}

void AutofillExternalDelegate::InsertDataListValues(
    std::vector<Suggestion>* suggestions) {
  if (data_list_values_.empty())
    return;

  // Go through the list of autocomplete values and remove them if they are in
  // the list of datalist values.
  std::set<base::string16> data_list_set(data_list_values_.begin(),
                                         data_list_values_.end());
  base::EraseIf(*suggestions, [&data_list_set](const Suggestion& suggestion) {
    return suggestion.frontend_id == POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY &&
           base::ContainsKey(data_list_set, suggestion.value);
  });

#if !defined(OS_ANDROID)
  // Insert the separator between the datalist and Autofill/Autocomplete values
  // (if there are any).
  if (!suggestions->empty()) {
    suggestions->insert(suggestions->begin(), Suggestion());
    (*suggestions)[0].frontend_id = POPUP_ITEM_ID_SEPARATOR;
  }
#endif

  // Insert the datalist elements at the beginning.
  suggestions->insert(suggestions->begin(), data_list_values_.size(),
                      Suggestion());
  for (size_t i = 0; i < data_list_values_.size(); i++) {
    (*suggestions)[i].value = data_list_values_[i];
    (*suggestions)[i].label = data_list_labels_[i];
    (*suggestions)[i].frontend_id = POPUP_ITEM_ID_DATALIST_ENTRY;
  }
}

base::string16 AutofillExternalDelegate::GetSettingsSuggestionValue()
    const {
  switch (GetPopupType()) {
    case PopupType::kAddresses:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_ADDRESSES);

    case PopupType::kCreditCards:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE_PAYMENT_METHODS);

    case PopupType::kPersonalInformation:
    case PopupType::kUnspecified:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE);

    case PopupType::kPasswords:
      NOTREACHED();
      return base::string16();
  }
}

}  // namespace autofill
