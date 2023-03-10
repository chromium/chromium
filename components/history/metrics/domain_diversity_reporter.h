// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_METRICS_DOMAIN_DIVERSITY_REPORTER_H_
#define COMPONENTS_HISTORY_METRICS_DOMAIN_DIVERSITY_REPORTER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_service_observer.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A profile keyed service responsible for scheduling periodic tasks to report
// domain diversity metrics.
class DomainDiversityReporter : public KeyedService,
                                public history::HistoryServiceObserver {
 public:
  DomainDiversityReporter(history::HistoryService* history_service,
                          PrefService* prefs,
                          base::Clock* clock);

  DomainDiversityReporter(const DomainDiversityReporter&) = delete;
  DomainDiversityReporter& operator=(const DomainDiversityReporter&) = delete;

  ~DomainDiversityReporter() override;

  // Registers Profile preferences in `registry`.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Invokes ComputeDomainMetrics() if history backend is already loaded.
  // Otherwise, use a HistoryServiceObserver to start ComputeDomainMetrics()
  // as soon as the backend is loaded.
  void MaybeComputeDomainMetrics();

  // Computes the domain diversity metric and emits histogram through callback,
  // and schedules another domain metric computation task for 24 hours later.
  void ComputeDomainMetrics();

  // Callback to emit histograms for domain metrics.
  // `result` is a pair of ("local-only", "local-and-synced") data - see
  // HistoryBackend::GetDomainDiversity().
  void ReportDomainMetrics(base::Time time_current_report_triggered,
                           std::pair<history::DomainDiversityResults,
                                     history::DomainDiversityResults> result);

  // HistoryServiceObserver:
  void OnHistoryServiceLoaded(
      history::HistoryService* history_service) override;
  void HistoryServiceBeingDeleted(
      history::HistoryService* history_service) override;

  // KeyedService implementation.
  void Shutdown() override {}

 private:
  raw_ptr<history::HistoryService> history_service_;
  raw_ptr<PrefService> prefs_;
  raw_ptr<base::Clock> clock_;

  base::ScopedObservation<history::HistoryService,
                          history::HistoryServiceObserver>
      history_service_observer_;
  base::CancelableTaskTracker cancelable_task_tracker_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DomainDiversityReporter> weak_ptr_factory_{this};
};

#endif  // COMPONENTS_HISTORY_METRICS_DOMAIN_DIVERSITY_REPORTER_H_
