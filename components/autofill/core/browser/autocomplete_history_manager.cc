// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autocomplete_history_manager.h"

#include <unordered_map>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_entry.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

using NotificationType = autofill::AutofillObserver::NotificationType;

namespace autofill {
namespace {

// Limit on the number of suggestions to appear in the pop-up menu under an
// text input element in a form.
const int kMaxAutocompleteMenuItems = 6;

bool IsTextField(const FormFieldData& field) {
  return
      field.form_control_type == "text" ||
      field.form_control_type == "search" ||
      field.form_control_type == "tel" ||
      field.form_control_type == "url" ||
      field.form_control_type == "email";
}

}  // namespace

void AutocompleteHistoryManager::UMARecorder::OnGetAutocompleteSuggestions(
    const base::string16& name,
    WebDataServiceBase::Handle pending_query_handle) {
  // log only if the current field is different than the latest one that has
  // been logged. we assume that user works at the same field if
  // measuring_name_ is same as the name of current field.
  bool should_log_query = measuring_name_ != name;

  if (should_log_query) {
    AutofillMetrics::LogAutocompleteQuery(pending_query_handle /* created */);
    measuring_name_ = name;
  }
  // We should track the query and log the suggestions, only if
  // - query has been logged.
  // - or, the query we previously tracked has been cancelled
  // The previous query must be cancelled if measuring_query_handle_ isn't
  // reset.
  if (should_log_query || measuring_query_handle_)
    measuring_query_handle_ = pending_query_handle;
}

void AutocompleteHistoryManager::UMARecorder::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle pending_query_handle,
    bool has_suggestion) {
  // If handle of completed query does not match the query we're currently
  // measuring then we've already logged a query for this name.
  bool was_already_logged = (pending_query_handle != measuring_query_handle_);
  measuring_query_handle_ = 0;
  if (was_already_logged)
    return;
  AutofillMetrics::LogAutocompleteSuggestions(has_suggestion);
}

AutocompleteHistoryManager::QueryHandler::QueryHandler(
    int client_query_id,
    bool autoselect_first_suggestion,
    base::string16 prefix,
    base::WeakPtr<SuggestionsHandler> handler)
    : client_query_id_(client_query_id),
      autoselect_first_suggestion_(autoselect_first_suggestion),
      prefix_(prefix),
      handler_(std::move(handler)) {}

AutocompleteHistoryManager::QueryHandler::QueryHandler(
    const QueryHandler& original) = default;

AutocompleteHistoryManager::QueryHandler::~QueryHandler() = default;

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

base::WeakPtr<AutocompleteHistoryManager>
AutocompleteHistoryManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutocompleteHistoryManager::OnGetAutocompleteSuggestions(
    int query_id,
    bool is_autocomplete_enabled,
    bool autoselect_first_suggestion,
    const base::string16& name,
    const base::string16& prefix,
    const std::string& form_control_type,
    base::WeakPtr<SuggestionsHandler> handler) {
  CancelPendingQueries(handler.get());

  if (!is_autocomplete_enabled || form_control_type == "textarea" ||
      IsInAutofillSuggestionsDisabledExperiment()) {
    SendSuggestions({}, QueryHandler(query_id, autoselect_first_suggestion,
                                     prefix, handler));
    uma_recorder_.OnGetAutocompleteSuggestions(name,
                                               0 /* pending_query_handle */);
    return;
  }

  if (profile_database_) {
    auto query_handle = profile_database_->GetFormValuesForElementName(
        name, prefix, kMaxAutocompleteMenuItems, this);
    uma_recorder_.OnGetAutocompleteSuggestions(name, query_handle);

    // We can simply insert, since |query_handle| is always unique.
    pending_queries_.insert(
        {query_handle,
         QueryHandler(query_id, autoselect_first_suggestion, prefix, handler)});
  }
}

void AutocompleteHistoryManager::OnWillSubmitForm(
    const FormData& form,
    bool is_autocomplete_enabled) {
  if (!is_autocomplete_enabled || is_off_the_record_) {
    Notify(NotificationType::AutocompleteFormSkipped);
    return;
  }

  std::vector<FormFieldData> values;
  for (const FormFieldData& field : form.fields) {
    if (IsFieldValueSaveable(field)) {
      values.push_back(field);
    }
  }

  if (!values.empty() && profile_database_.get()) {
    profile_database_->AddFormFields(values);

    Notify(NotificationType::AutocompleteFormSubmitted);
  }
}

void AutocompleteHistoryManager::OnRemoveAutocompleteEntry(
    const base::string16& name, const base::string16& value) {
  if (profile_database_)
    profile_database_->RemoveFormValueForElementName(name, value);
}

void AutocompleteHistoryManager::OnAutocompleteEntrySelected(
    const base::string16& value) {
  // Try to find the AutofillEntry associated with the given suggestion.
  auto last_entries_iter = last_entries_.find(value);
  if (last_entries_iter == last_entries_.end()) {
    // Not found, therefore nothing to do. Most likely there was a race
    // condition, but it's not that big of a deal in the current scenario
    // (logging metrics).
    NOTREACHED();
    return;
  }

  // The AutofillEntry was found, use it to log the DaysSinceLastUsed.
  const AutofillEntry& entry = last_entries_iter->second;
  base::TimeDelta time_delta = AutofillClock::Now() - entry.date_last_used();
  AutofillMetrics::LogAutocompleteDaysSinceLastUse(time_delta.InDays());
}

void AutocompleteHistoryManager::CancelPendingQueries(
    const SuggestionsHandler* handler) {
  if (handler && profile_database_) {
    for (auto iter : pending_queries_) {
      const QueryHandler& query_handler = iter.second;

      if (query_handler.handler_ && query_handler.handler_.get() == handler) {
        profile_database_->CancelRequest(iter.first);
      }
    }
  }

  // Cleaning up the map with the cancelled handler to remove cancelled
  // requests.
  CleanupEntries(handler);
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

void AutocompleteHistoryManager::SendSuggestions(
    const std::vector<AutofillEntry>& entries,
    const QueryHandler& query_handler) {
  if (!query_handler.handler_) {
    // Either the handler has been destroyed, or it is invalid.
    return;
  }

  // If there is only one suggestion that is the exact same string as
  // what is in the input box, then don't show the suggestion.
  bool hide_suggestions =
      entries.size() == 1 && query_handler.prefix_ == entries[0].key().value();

  std::vector<Suggestion> suggestions;
  last_entries_.clear();

  if (!hide_suggestions) {
    for (const AutofillEntry& entry : entries) {
      suggestions.push_back(Suggestion(entry.key().value()));
      last_entries_.insert({entry.key().value(), AutofillEntry(entry)});
    }
  }

  query_handler.handler_->OnSuggestionsReturned(
      query_handler.client_query_id_,
      query_handler.autoselect_first_suggestion_, suggestions);
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

  const WDResult<std::vector<AutofillEntry>>* autofill_result =
      static_cast<const WDResult<std::vector<AutofillEntry>>*>(result.get());
  std::vector<AutofillEntry> entries = autofill_result->GetValue();
  SendSuggestions(entries, query_handler);
  uma_recorder_.OnWebDataServiceRequestDone(
      current_handle, !entries.empty() /* has_suggestion */);
}

void AutocompleteHistoryManager::OnAutofillCleanupReturned(
    WebDataServiceBase::Handle current_handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(result);
  DCHECK_EQ(AUTOFILL_CLEANUP_RESULT, result->GetType());

  const WDResult<size_t>* cleanup_wdresult =
      static_cast<const WDResult<size_t>*>(result.get());

  AutofillMetrics::LogNumberOfAutocompleteEntriesCleanedUp(
      cleanup_wdresult->GetValue());

  // Cleanup was successful, update the latest run milestone.
  pref_service_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                            CHROME_VERSION_MAJOR);

  Notify(NotificationType::AutocompleteCleanupDone);
}

void AutocompleteHistoryManager::CancelAllPendingQueries() {
  if (profile_database_) {
    for (const auto& pending_query : pending_queries_) {
      profile_database_->CancelRequest(pending_query.first);
    }
  }

  pending_queries_.clear();
}

void AutocompleteHistoryManager::CleanupEntries(
    const SuggestionsHandler* handler) {
  base::EraseIf(pending_queries_, [handler](const auto& pending_query) {
    const QueryHandler& query_handler = pending_query.second;
    return !query_handler.handler_ || query_handler.handler_.get() == handler;
  });
}

// We put the following restriction on stored FormFields:
//  - non-empty name
//  - non-empty nor whitespace only value
//  - text field
//  - autocomplete is not disabled
//  - value is not a credit card number
//  - value is not a SSN
//  - field was not identified as a CVC field (this is handled in
//    AutofillManager)
//  - field is focusable
//  - not a presentation field
bool AutocompleteHistoryManager::IsFieldValueSaveable(
    const FormFieldData& field) {
  // We don't want to save a trimmed string, but we want to make sure that the
  // value is non-empty nor only whitespaces.
  bool is_value_valid = false;
  for (const char& c : field.value) {
    if (c != ' ') {
      is_value_valid = true;
      break;
    }
  }

  return is_value_valid && !field.name.empty() && IsTextField(field) &&
         field.should_autocomplete && !IsValidCreditCardNumber(field.value) &&
         !IsSSN(field.value) && field.is_focusable &&
         field.role != FormFieldData::RoleAttribute::kPresentation;
}

}  // namespace autofill
