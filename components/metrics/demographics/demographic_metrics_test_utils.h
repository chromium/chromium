// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_DEMOGRAPHICS_DEMOGRAPHIC_METRICS_TEST_UTILS_H_
#define COMPONENTS_METRICS_DEMOGRAPHICS_DEMOGRAPHIC_METRICS_TEST_UTILS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/metrics/metrics_log_store.h"
#include "components/metrics/metrics_service.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_service.h"
#include "components/sync/test/fake_server.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

// Helpers to support testing the reporting of user demographic metrics in
// browser tests.

namespace metrics {
namespace test {

// Parameters for the parameterized tests.
struct DemographicsTestParams {
  // Enable the feature to report the user's birth year and gender.
  bool enable_feature = false;
  // Expectation for the user's noised birth year and gender to be reported.
  // Having |enable_feature| set to true does not necessarily mean that
  // |expect_reported_demographics| will be true because other conditions might
  // stop the reporting of the user's noised birth year and gender, e.g.,
  // sync is turned off.
  bool expect_reported_demographics = false;
};

// Adds the User Demographic priority pref to the sync |fake_server|, which
// contains the synced test user's raw, i.e. un-noised, |birth_year| and
// |gender|.
void AddUserBirthYearAndGenderToSyncServer(
    base::WeakPtr<fake_server::FakeServer> fake_server,
    int birth_year,
    UserDemographicsProto::Gender gender);

// Updates the network time to approximately |now|.
void UpdateNetworkTime(const base::Time& now,
                       network_time::NetworkTimeTracker* time_tracker);

// Returns the maximum eligible birth year for the given time. The returned year
// is inclusive; i.e. years <= the returned year are eligible. In  order to
// compute the synced test user's age, the network time should have already been
// set to |now|.
int GetMaximumEligibleBirthYear(const base::Time& now);

// Gets the noised birth year of the user, where the |raw_birth_year|
// is the true birth year, pre-noise, and |local_state| is the service with the
// user's noise pref. This function should be run only after a
// DemographicMetricsProvider has provided user demographics to a report.
int GetNoisedBirthYear(const PrefService* local_state, int raw_birth_year);

// If data are available, creates an UMA log and stores it in the
// MetricsService's MetricsLogStore.
void BuildAndStoreLog(MetricsService* metrics_service);

// Returns true if |metrics_service|'s log store has logs to send.
bool HasUnsentLogs(MetricsService* metrics_service);

// Returns an UMA log if the MetricsService has a staged log.
std::unique_ptr<ChromeUserMetricsExtension> GetLastUmaLog(
    MetricsService* metrics_service);

}  // namespace test
}  // namespace metrics

#endif  // COMPONENTS_METRICS_DEMOGRAPHICS_DEMOGRAPHIC_METRICS_TEST_UTILS_H_
