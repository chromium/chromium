// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/core/counters/autofill_counter.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"

namespace {

bool IsAutofillSyncEnabled(const syncer::SyncService* sync_service) {
  return sync_service &&
         sync_service->GetUserSettings()->IsFirstSetupComplete() &&
         sync_service->IsSyncFeatureActive() &&
         sync_service->GetActiveDataTypes().Has(syncer::AUTOFILL);
}

}  // namespace

namespace browsing_data {

AutofillCounter::AutofillCounter(
    scoped_refptr<autofill::AutofillWebDataService> web_data_service,
    syncer::SyncService* sync_service)
    : web_data_service_(web_data_service),
      sync_tracker_(this, sync_service),
      suggestions_query_(0),
      credit_cards_query_(0),
      addresses_query_(0),
      num_suggestions_(0),
      num_credit_cards_(0),
      num_addresses_(0) {}

AutofillCounter::~AutofillCounter() {
  CancelAllRequests();
}

void AutofillCounter::OnInitialized() {
  DCHECK(web_data_service_);
  sync_tracker_.OnInitialized(base::Bind(&IsAutofillSyncEnabled));
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

  CancelAllRequests();

  // Count the autocomplete suggestions (also called form elements in Autofill).
  // Note that |AutofillTable::RemoveFormElementsAddedBetween| only deletes
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
  suggestions_query_ =
      web_data_service_->GetCountOfValuesContainedBetween(start, end, this);

  // Count the credit cards.
  credit_cards_query_ = web_data_service_->GetCreditCards(this);

  // Count the addresses.
  addresses_query_ = web_data_service_->GetAutofillProfiles(this);
}

void AutofillCounter::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle handle,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!result) {
    // CancelAllRequests will cancel all queries that are active; the query that
    // just failed is complete and cannot be canceled so zero it out.
    if (handle == suggestions_query_) {
      suggestions_query_ = 0;
    } else if (handle == credit_cards_query_) {
      credit_cards_query_ = 0;
    } else if (handle == addresses_query_) {
      addresses_query_ = 0;
    } else {
      NOTREACHED();
    }

    CancelAllRequests();
    return;
  }

  const base::Time start = period_start_for_testing_.is_null()
                               ? GetPeriodStart()
                               : period_start_for_testing_;
  const base::Time end = period_end_for_testing_.is_null()
                             ? GetPeriodEnd()
                             : period_end_for_testing_;

  if (handle == suggestions_query_) {
    // Autocomplete suggestions.
    DCHECK_EQ(AUTOFILL_VALUE_RESULT, result->GetType());
    num_suggestions_ =
        static_cast<const WDResult<int>*>(result.get())->GetValue();
    suggestions_query_ = 0;

  } else if (handle == credit_cards_query_) {
    // Credit cards.
    DCHECK_EQ(AUTOFILL_CREDITCARDS_RESULT, result->GetType());
    auto credit_cards =
        static_cast<
            WDResult<std::vector<std::unique_ptr<autofill::CreditCard>>>*>(
            result.get())
            ->GetValue();

    num_credit_cards_ = std::count_if(
        credit_cards.begin(), credit_cards.end(),
        [start, end](const std::unique_ptr<autofill::CreditCard>& card) {
          return (card->modification_date() >= start &&
                  card->modification_date() < end);
        });
    credit_cards_query_ = 0;

  } else if (handle == addresses_query_) {
    // Addresses.
    DCHECK_EQ(AUTOFILL_PROFILES_RESULT, result->GetType());
    auto addresses =
        static_cast<
            WDResult<std::vector<std::unique_ptr<autofill::AutofillProfile>>>*>(
            result.get())
            ->GetValue();

    num_addresses_ = std::count_if(
        addresses.begin(), addresses.end(),
        [start,
         end](const std::unique_ptr<autofill::AutofillProfile>& address) {
          return (address->modification_date() >= start &&
                  address->modification_date() < end);
        });
    addresses_query_ = 0;

  } else {
    NOTREACHED() << "No such query: " << handle;
  }

  // If we still have pending queries, do not report data yet.
  if (suggestions_query_ || credit_cards_query_ || addresses_query_)
    return;

  auto reported_result = std::make_unique<AutofillResult>(
      this, num_suggestions_, num_credit_cards_, num_addresses_,
      sync_tracker_.IsSyncActive());
  ReportResult(std::move(reported_result));
}

void AutofillCounter::CancelAllRequests() {
  if (suggestions_query_)
    web_data_service_->CancelRequest(suggestions_query_);
  if (credit_cards_query_)
    web_data_service_->CancelRequest(credit_cards_query_);
  if (addresses_query_)
    web_data_service_->CancelRequest(addresses_query_);
}

// AutofillCounter::AutofillResult ---------------------------------------------

AutofillCounter::AutofillResult::AutofillResult(const AutofillCounter* source,
                                                ResultInt num_suggestions,
                                                ResultInt num_credit_cards,
                                                ResultInt num_addresses,
                                                bool autofill_sync_enabled_)
    : SyncResult(source, num_suggestions, autofill_sync_enabled_),
      num_credit_cards_(num_credit_cards),
      num_addresses_(num_addresses) {}

AutofillCounter::AutofillResult::~AutofillResult() {}

}  // namespace browsing_data
