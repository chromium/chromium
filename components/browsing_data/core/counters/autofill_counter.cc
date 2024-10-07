// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/autofill_counter.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"

namespace {

bool IsAutocompleteSyncActive(const syncer::SyncService* sync_service) {
  return sync_service &&
         sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL);
}

}  // namespace

namespace browsing_data {

AutofillCounter::AutofillCounter(
    autofill::PersonalDataManager* personal_data_manager,
    scoped_refptr<autofill::AutofillWebDataService> web_data_service,
    user_annotations::UserAnnotationsService* user_annotations_service,
    syncer::SyncService* sync_service)
    : personal_data_manager_(personal_data_manager),
      web_data_service_(web_data_service),
      user_annotations_service_(user_annotations_service),
      sync_tracker_(this, sync_service),
      suggestions_query_(0),
      num_suggestions_(0) {}

AutofillCounter::~AutofillCounter() {
  CancelAllRequests();
}

void AutofillCounter::OnInitialized() {
  DCHECK(personal_data_manager_);
  DCHECK(web_data_service_);
  sync_tracker_.OnInitialized(base::BindRepeating(&IsAutocompleteSyncActive));
}

const char* AutofillCounter::GetPrefName() const {
  return browsing_data::prefs::kDeleteFormData;
}

void AutofillCounter::SetPeriodStartForTesting(
    const base::Time& period_start_for_testing) {
  period_start_for_testing_ = period_start_for_testing;
}

void AutofillCounter::SetPeriodEndForTesting(
    const base::Time& period_end_for_testing) {
  period_end_for_testing_ = period_end_for_testing;
}

void AutofillCounter::Count() {
  const base::Time start = period_start_for_testing_.is_null()
                               ? GetPeriodStart()
                               : period_start_for_testing_;
  const base::Time end = period_end_for_testing_.is_null()
                             ? GetPeriodEnd()
                             : period_end_for_testing_;

  // Credit cards.
  num_credit_cards_ = base::ranges::count_if(
      personal_data_manager_->payments_data_manager().GetLocalCreditCards(),
      [start, end](const autofill::CreditCard* card) {
        return (card->modification_date() >= start &&
                card->modification_date() < end);
      });

  // Addresses.
  num_addresses_ = base::ranges::count_if(
      personal_data_manager_->address_data_manager().GetProfilesByRecordType(
          autofill::AutofillProfile::RecordType::kLocalOrSyncable),
      [start, end](const autofill::AutofillProfile* address) {
        return (address->modification_date() >= start &&
                address->modification_date() < end);
      });

  CancelAllRequests();

  // Count the autocomplete suggestions (also called form elements in Autofill).
  // Note that |AutocompleteTable::RemoveFormElementsAddedBetween| only deletes
  // those whose entire existence (i.e. the interval between creation time
  // and last modified time) lies within the deletion time range. Otherwise,
  // it only decreases the count property, but always to a nonzero value,
  // and the suggestion is retained. Therefore here as well, we must only count
  // the entries that are entirely contained in [start, end).
  // Further, many of these entries may contain the same values, as they are
  // simply the same data point entered on different forms. For example,
  // [name, value] pairs such as:
  //     ["mail", "example@example.com"]
  //     ["email", "example@example.com"]
  //     ["e-mail", "example@example.com"]
  // are stored as three separate entries, but from the user's perspective,
  // they constitute the same suggestion - "my email". Therefore, for the final
  // output, we will consider all entries with the same value as one suggestion,
  // and increment the counter only if all entries with the given value are
  // contained in the interval [start, end).
  // The `num_suggestion_` is reset to denote that new data is awaited.
  num_suggestions_.reset();
  suggestions_query_ =
      web_data_service_->GetCountOfValuesContainedBetween(start, end, this);

  num_user_annotations_.reset();

  // Not all platforms support user annotations, for those the service is not
  // provided and the value is set to 0 immediately.
  if (user_annotations_service_) {
    user_annotations_service_->GetCountOfValuesContainedBetween(
        start, end,
        base::BindOnce(&AutofillCounter::OnUserAnnotationsServiceResponse,
                       user_annotations_requirest_weak_factory_.GetWeakPtr()));
  } else {
    num_user_annotations_ = 0;
  }
}

void AutofillCounter::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(handle, suggestions_query_);
  suggestions_query_ = 0;

  if (!result) {
    return;
  }

  // Autocomplete suggestions.
  DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
  num_suggestions_ =
      static_cast<const WDResult<int>*>(result.get())->GetValue();

  ReportResultIfReady();
}

void AutofillCounter::OnUserAnnotationsServiceResponse(
    int num_user_annotations) {
  num_user_annotations_ = num_user_annotations;

  ReportResultIfReady();
}

void AutofillCounter::CancelAllRequests() {
  if (suggestions_query_)
    web_data_service_->CancelRequest(suggestions_query_);

  user_annotations_requirest_weak_factory_.InvalidateWeakPtrs();
}

void AutofillCounter::ReportResultIfReady() {
  if (num_suggestions_.has_value() && num_user_annotations_.has_value()) {
    auto reported_result = std::make_unique<AutofillResult>(
        this, *num_suggestions_, num_credit_cards_, num_addresses_,
        *num_user_annotations_, sync_tracker_.IsSyncActive());
    ReportResult(std::move(reported_result));
  }
}

// AutofillCounter::AutofillResult ---------------------------------------------

AutofillCounter::AutofillResult::AutofillResult(
    const AutofillCounter* source,
    ResultInt num_suggestions,
    ResultInt num_credit_cards,
    ResultInt num_addresses,
    ResultInt num_user_annotation_entries,
    bool autofill_sync_enabled_)
    : SyncResult(source, num_suggestions, autofill_sync_enabled_),
      num_credit_cards_(num_credit_cards),
      num_addresses_(num_addresses),
      num_user_annotation_entries_(num_user_annotation_entries) {}

AutofillCounter::AutofillResult::~AutofillResult() {}

}  // namespace browsing_data
