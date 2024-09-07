// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autocomplete_history_manager.h"

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/credit_card_number_validation.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace autofill {

namespace {

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
const int kMaxAutocompleteMenuItems = 6;

// Returns true if the field has a meaningful name.
// An input field name 'field_2' bears no semantic meaning and there is a chance
// that a different website or different form uses the same field name for a
// totally different purpose.
bool IsMeaningfulFieldName(const std::u16string& name) {
  static constexpr char16_t kRegex[] =
      u"^(((field|input|mat-input)(_|-)?\\d+)|title|otp|tan)$|"
      u"(cvc|cvn|cvv|captcha)";
  return !MatchesRegex<kRegex>(name);
}

}  // namespace

AutocompleteHistoryManager::AutocompleteHistoryManager()
    // It is safe to base::Unretained a raw pointer to the current instance,
    // as it is already being owned elsewhere and will be cleaned-up properly.
    // Also, the map of callbacks will be deleted when this instance is
    // destroyed, which means we won't attempt to run one of these callbacks
    // beyond the life of this instance.
    : request_callbacks_(
          {{AUTOFILL_VALUE_RESULT,
            base::BindRepeating(
                &AutocompleteHistoryManager::OnAutofillValuesReturned,
                base::Unretained(this))},
           {AUTOFILL_CLEANUP_RESULT,
            base::BindRepeating(
                &AutocompleteHistoryManager::OnAutofillCleanupReturned,
                base::Unretained(this))}}) {}

AutocompleteHistoryManager::~AutocompleteHistoryManager() {
  CancelAllPendingQueries();
}

bool AutocompleteHistoryManager::OnGetSingleFieldSuggestions(
    const FormStructure* form_structure,
    const FormFieldData& field,
    const AutofillField* autofill_field,
    const AutofillClient& client,
    OnSuggestionsReturnedCallback on_suggestions_returned) {
  if (!field.should_autocomplete()) {
    return false;
  }

  CancelPendingQueries();

  if (!IsMeaningfulFieldName(field.name()) || !client.IsAutocompleteEnabled() ||
      field.form_control_type() == FormControlType::kTextArea ||
      field.form_control_type() == FormControlType::kContentEditable ||
      IsInAutofillSuggestionsDisabledExperiment()) {
    SendSuggestions({}, QueryHandler(field.global_id(), field.value(),
                                     std::move(on_suggestions_returned)));
    return true;
  }

  if (profile_database_) {
    auto query_handle = profile_database_->GetFormValuesForElementName(
        field.name(), field.value(), kMaxAutocompleteMenuItems, this);

    // We can simply insert, since |query_handle| is always unique.
    pending_queries_.insert(
        {query_handle, QueryHandler(field.global_id(), field.value(),
                                    std::move(on_suggestions_returned))});
    return true;
  }
  return false;
}

void AutocompleteHistoryManager::OnWillSubmitFormWithFields(
    const std::vector<FormFieldData>& fields,
    bool is_autocomplete_enabled) {
  if (!is_autocomplete_enabled || is_off_the_record_) {
    return;
  }
  std::vector<FormFieldData> autocomplete_saveable_fields;
  autocomplete_saveable_fields.reserve(fields.size());
  for (const FormFieldData& field : fields) {
    if (IsFieldValueSaveable(field)) {
      autocomplete_saveable_fields.push_back(field);
    }
  }
  if (!autocomplete_saveable_fields.empty() && profile_database_.get()) {
    profile_database_->AddFormFields(autocomplete_saveable_fields);
  }
}

void AutocompleteHistoryManager::CancelPendingQueries() {
  if (profile_database_) {
    for (const auto& [handle, query_handler] : pending_queries_) {
      profile_database_->CancelRequest(handle);
    }
  }
  pending_queries_.clear();
}

void AutocompleteHistoryManager::OnRemoveCurrentSingleFieldSuggestion(
    const std::u16string& field_name,
    const std::u16string& value,
    SuggestionType type) {
  if (profile_database_)
    profile_database_->RemoveFormValueForElementName(field_name, value);
}

void AutocompleteHistoryManager::OnSingleFieldSuggestionSelected(
    const Suggestion& suggestion) {
  // Try to find the AutofillEntry associated with the given suggestion.
  auto last_entries_iter = last_entries_.find(suggestion.main_text.value);
  if (last_entries_iter == last_entries_.end()) {
    // Not found, therefore nothing to do. Most likely there was a race
    // condition, but it's not that big of a deal in the current scenario
    // (logging metrics).
    DUMP_WILL_BE_NOTREACHED();
    return;
  }

  // The AutocompleteEntry was found, use it to log the DaysSinceLastUsed.
  const AutocompleteEntry& entry = last_entries_iter->second;
  base::TimeDelta time_delta = AutofillClock::Now() - entry.date_last_used();
  AutofillMetrics::LogAutocompleteDaysSinceLastUse(time_delta.InDays());
}

void AutocompleteHistoryManager::Init(
    scoped_refptr<AutofillWebDataService> profile_database,
    PrefService* pref_service,
    bool is_off_the_record) {
  profile_database_ = profile_database;
  pref_service_ = pref_service;
  is_off_the_record_ = is_off_the_record;

  if (!profile_database_) {
    // In some tests, there are no dbs.
    return;
  }

  // No need to run the retention policy in OTR.
  if (!is_off_the_record_) {
    // Upon successful cleanup, the last cleaned-up major version is being
    // stored in this pref.
    int last_cleaned_version = pref_service_->GetInteger(
        prefs::kAutocompleteLastVersionRetentionPolicy);
    if (CHROME_VERSION_MAJOR > last_cleaned_version) {
      // Trigger the cleanup.
      profile_database_->RemoveExpiredAutocompleteEntries(this);
    }
  }
}

void AutocompleteHistoryManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(current_handle);

  if (!result) {
    // Returning early here if |result| is null.  We've seen this happen on
    // Linux due to NFS dismounting and causing sql failures.
    // See http://crbug.com/68783.
    return;
  }

  WDResultType result_type = result->GetType();

  auto request_callbacks_iter = request_callbacks_.find(result_type);
  if (request_callbacks_iter == request_callbacks_.end()) {
    // There are no callbacks for this response, hence nothing to do.
    return;
  }

  request_callbacks_iter->second.Run(current_handle, std::move(result));
}

AutocompleteHistoryManager::QueryHandler::QueryHandler(
    FieldGlobalId field_id,
    std::u16string prefix,
    OnSuggestionsReturnedCallback on_suggestions_returned)
    : field_id_(field_id),
      prefix_(std::move(prefix)),
      on_suggestions_returned_(std::move(on_suggestions_returned)) {}

AutocompleteHistoryManager::QueryHandler::QueryHandler(QueryHandler&&) =
    default;

AutocompleteHistoryManager::QueryHandler::~QueryHandler() = default;

void AutocompleteHistoryManager::SendSuggestions(
    const std::vector<AutocompleteEntry>& entries,
    QueryHandler query_handler) {
  // If there is only one suggestion that is the exact same string as
  // what is in the input box, then don't show the suggestion.
  bool hide_suggestions =
      entries.size() == 1 && query_handler.prefix_ == entries[0].key().value();

  std::vector<Suggestion> suggestions;
  last_entries_.clear();

  if (!hide_suggestions) {
    for (const AutocompleteEntry& entry : entries) {
      suggestions.push_back(Suggestion(entry.key().value()));
      last_entries_.insert({entry.key().value(), AutocompleteEntry(entry)});
    }
  }

  std::move(query_handler.on_suggestions_returned_)
      .Run(query_handler.field_id_, suggestions);
}

void AutocompleteHistoryManager::CancelAllPendingQueries() {
  if (profile_database_) {
    for (const auto& [handle, query_handler] : pending_queries_) {
      profile_database_->CancelRequest(handle);
    }
  }

  pending_queries_.clear();
}

void AutocompleteHistoryManager::OnAutofillValuesReturned(
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(result);
  DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());

  auto pending_queries_iter = pending_queries_.find(current_handle);
  if (pending_queries_iter == pending_queries_.end()) {
    // There's no handler for this query, hence nothing to do.
    return;
  }

  // Moving the handler since we're erasing the entry.
  auto query_handler = std::move(pending_queries_iter->second);

  // Removing the query, as it is no longer pending.
  pending_queries_.erase(pending_queries_iter);

  const WDResult<std::vector<AutocompleteEntry>>* autocomplete_result =
      static_cast<const WDResult<std::vector<AutocompleteEntry>>*>(
          result.get());
  std::vector<AutocompleteEntry> entries = autocomplete_result->GetValue();
  SendSuggestions(entries, std::move(query_handler));
}

void AutocompleteHistoryManager::OnAutofillCleanupReturned(
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(result);
  DCHECK_EQ(AUTOFILL_CLEANUP_RESULT, result->GetType());

  // Cleanup was successful, update the latest run milestone.
  pref_service_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                            CHROME_VERSION_MAJOR);
}

// We put the following restriction on stored FormFields:
//  - non-empty name
//  - neither empty nor whitespace-only value
//  - text field
//  - autocomplete is not disabled
//  - value is not a credit card number
//  - field has user typed input or is focusable (this is a mild criteria but
//    this way it is consistent for all platforms)
//  - not a presentation field
bool AutocompleteHistoryManager::IsFieldValueSaveable(
    const FormFieldData& field) {
  // We don't want to save a trimmed string, but we want to make sure that the
  // value is neither empty nor only whitespaces.
  bool is_value_valid = std::ranges::any_of(
      field.value(), std::not_fn(base::IsUnicodeWhitespace<char16_t>));
  return is_value_valid && IsMeaningfulFieldName(field.name()) &&
         !field.name().empty() && field.IsTextInputElement() &&
         !field.IsPasswordInputElement() &&
         field.form_control_type() != FormControlType::kInputNumber &&
         field.should_autocomplete() &&
         !IsValidCreditCardNumber(field.value()) && !IsSSN(field.value()) &&
         (field.properties_mask() & kUserTyped || field.is_focusable()) &&
         field.role() != FormFieldData::RoleAttribute::kPresentation;
}

}  // namespace autofill
