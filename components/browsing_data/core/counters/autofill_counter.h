// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CORE_COUNTERS_AUTOFILL_COUNTER_H_
#define COMPONENTS_BROWSING_DATA_CORE_COUNTERS_AUTOFILL_COUNTER_H_

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browsing_data/core/counters/browsing_data_counter.h"
#include "components/browsing_data/core/counters/sync_tracker.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}

namespace browsing_data {

class AutofillCounter : public browsing_data::BrowsingDataCounter,
                        public WebDataServiceConsumer {
 public:
  class AutofillResult : public SyncResult {
   public:
    AutofillResult(const AutofillCounter* source,
                   ResultInt num_suggestions,
                   ResultInt num_credit_cards,
                   ResultInt num_addresses,
                   bool autofill_sync_enabled_);

    AutofillResult(const AutofillResult&) = delete;
    AutofillResult& operator=(const AutofillResult&) = delete;

    ~AutofillResult() override;

    ResultInt num_credit_cards() const { return num_credit_cards_; }
    ResultInt num_addresses() const { return num_addresses_; }

   private:
    ResultInt num_credit_cards_;
    ResultInt num_addresses_;
  };

  AutofillCounter(
      autofill::PersonalDataManager* personal_data_manager,
      scoped_refptr<autofill::AutofillWebDataService> web_data_service,
      syncer::SyncService* sync_service);

  AutofillCounter(const AutofillCounter&) = delete;
  AutofillCounter& operator=(const AutofillCounter&) = delete;

  ~AutofillCounter() override;

  // BrowsingDataCounter implementation.
  void OnInitialized() override;

  const char* GetPrefName() const override;

  // Set the beginning of the time period for testing. AutofillTable does not
  // allow us to set time explicitly, and BrowsingDataCounter recognizes
  // only predefined time periods, out of which the lowest one is one hour.
  // Obviously, the test cannot run that long.
  // TODO(msramek): Consider changing BrowsingDataCounter to use arbitrary
  // time periods instead of BrowsingDataRemover::TimePeriod.
  void SetPeriodStartForTesting(const base::Time& period_start_for_testing);

  // Set the ending of the time period for testing.
  void SetPeriodEndForTesting(const base::Time& period_end_for_testing);

 private:
  void Count() override;

  // WebDataServiceConsumer implementation.
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle handle,
      std::unique_ptr<WDTypedResult> result) override;

  // Cancel all pending requests to AutofillWebdataService.
  void CancelAllRequests();

  base::ThreadChecker thread_checker_;

  raw_ptr<autofill::PersonalDataManager> personal_data_manager_;
  scoped_refptr<autofill::AutofillWebDataService> web_data_service_;
  SyncTracker sync_tracker_;

  WebDataServiceBase::Handle suggestions_query_;

  ResultInt num_suggestions_;
  ResultInt num_credit_cards_;
  ResultInt num_addresses_;

  base::Time period_start_for_testing_;
  base::Time period_end_for_testing_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COUNTERS_AUTOFILL_COUNTER_H_
