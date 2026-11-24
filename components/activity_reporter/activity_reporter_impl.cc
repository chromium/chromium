// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/activity_reporter/activity_reporter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/activity_reporter/activity_reporter_for_testing.h"
#include "components/activity_reporter/buildflags.h"
#include "components/activity_reporter/configurator.h"
#include "components/activity_reporter/constants.h"
#include "components/activity_reporter/features.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/activity_data_service.h"
#include "components/update_client/network.h"
#include "components/update_client/persisted_data.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"

#if BUILDFLAG(IS_WIN)
#include "components/activity_reporter/virtual_machine_win.h"
#endif

namespace activity_reporter {

namespace {

// The exclusive maximum of the `ActivityReporter.IncompleteReports` histogram:
// buckets 0 through 10, with an overflow bucket for larger counts.
constexpr int kIncompleteReportsExclusiveMax = 11;

// The minimum time between two reports.
constexpr base::TimeDelta kMinReportInterval = base::Hours(5);

// The exclusive maximum of the `ActivityReporter.PingFreshnessAge` histogram.
constexpr base::TimeDelta kPingFreshnessAgeExclusiveMax = base::Days(28);

// The bucket count of the `ActivityReporter.PingFreshnessAge` histogram, which
// makes its buckets 1 to 3 hours wide up to 3 days, and starts a bucket at 53
// hours.
constexpr int kPingFreshnessAgeBucketCount = 100;

InitialDateLastActiveState ToState(int dla) {
  if (dla == update_client::kDateUnknown) {
    return InitialDateLastActiveState::kUnknown;
  }
  if (dla == update_client::kDateFirstTime) {
    return InitialDateLastActiveState::kFirstTimeUnpersisted;
  }
  if (dla >= 0) {
    return InitialDateLastActiveState::kPersisted;
  }
  return InitialDateLastActiveState::kInvalid;
}

class ActivityReporterImpl final : public ActivityReporter {
 public:
  ActivityReporterImpl(
      base::RepeatingCallback<PrefService*()> pref_service_provider,
      scoped_refptr<update_client::NetworkFetcherFactory>
          network_fetcher_factory,
      base::RepeatingCallback<version_info::Channel()> channel_provider,
      base::RepeatingCallback<std::string()> brand_provider,
      base::RepeatingClosure updater_active_callback,
      bool per_user_install)
      : configurator_(base::MakeRefCounted<ActivityReporterConfigurator>(
            std::move(pref_service_provider),
            std::move(network_fetcher_factory),
            channel_provider,
            per_user_install)),
        update_client_(update_client::UpdateClientFactory(configurator_)),
        updater_active_callback_(std::move(updater_active_callback)),
        channel_provider_(std::move(channel_provider)),
        brand_provider_(std::move(brand_provider)),
        clock_(base::DefaultClock::GetInstance()) {}

  ActivityReporterImpl(
      base::RepeatingCallback<PrefService*()> pref_service_provider,
      scoped_refptr<update_client::UpdateClient> update_client,
      base::RepeatingCallback<version_info::Channel()> channel_provider,
      base::RepeatingCallback<std::string()> brand_provider,
      base::RepeatingClosure updater_active_callback,
      const base::Clock* clock)
      :  // Provides the persisted data and the pref service that this reporter
         // uses. The injected `update_client` doesn't use this configurator.
        configurator_(base::MakeRefCounted<ActivityReporterConfigurator>(
            std::move(pref_service_provider),
            /*network_fetcher_factory=*/nullptr,
            channel_provider,
            /*per_user_install=*/false)),
        update_client_(std::move(update_client)),
        updater_active_callback_(std::move(updater_active_callback)),
        channel_provider_(std::move(channel_provider)),
        brand_provider_(std::move(brand_provider)),
        clock_(clock) {
    CHECK(clock_);
  }

  ~ActivityReporterImpl() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    base::UmaHistogramExactLinear("ActivityReporter.IncompleteReports",
                                  incomplete_reports_,
                                  kIncompleteReportsExclusiveMax);
  }

  void ReportActive() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if !BUILDFLAG(USE_LEGACY_ACTIVE_DEFINITION)
    // If updater-reported actives use the non-legacy active definition (this
    // one) report activity to the updater here.
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        updater_active_callback_);
#endif
    const base::Time now = clock_->Now();
    // A last report time in the future means the clock moved back, so it can't
    // throttle this report.
    if (last_reported_ && *last_reported_ <= now &&
        now - *last_reported_ < kMinReportInterval) {
      // The last report was too recent; don't send another.
      return;
    }
    if (!last_reported_) {
      PrepareFirstReport();
    }
    last_reported_ = now;
#if BUILDFLAG(IS_WIN)
    // Recorded once per Chrome active ping sent, so that the samples are
    // weighted by active use.
    RecordVirtualMachineHistogramsAsync();
#endif
    RecordAndRotatePingFreshness(now);
    ++incomplete_reports_;
    update_client_->CheckForUpdate(
        std::string(kChromeActivityId),
        base::BindOnce(
            [](version_info::Channel channel, const std::string& brand,
               const std::vector<std::string>& ids,
               base::OnceCallback<void(const std::vector<std::optional<
                                           update_client::CrxComponent>>&)>
                   callback) {
              std::vector<std::optional<update_client::CrxComponent>>
                  components;
              for (const std::string& id : ids) {
                if (id == kChromeActivityId) {
                  components.emplace_back(update_client::CrxComponent{
                      .app_id = std::string(kChromeActivityId),
                      .version = version_info::GetVersion(),
                      .brand = brand,
                      .updates_enabled = false,
                      .channel =
                          std::string(version_info::GetChannelString(channel)),
                  });
                } else {
                  components.push_back(std::nullopt);
                }
              }
              std::move(callback).Run(components);
            },
            channel_provider_.Run(), brand_provider_.Run()),
        base::DoNothing(), false,
        base::BindOnce(&ActivityReporterImpl::OnCheckForUpdateComplete,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  // Records `ActivityReporter.InitialDateLastActiveState`, then marks the first
  // report of this session as the browser's first-ever active report if the
  // browser has no persisted DateLastActive.
  void PrepareFirstReport() {
    update_client::PersistedData* data = configurator_->GetPersistedData();
    const std::string app_id(kChromeActivityId);
    const int date_last_active = data->GetDateLastActive(app_id);
    base::UmaHistogramEnumeration("ActivityReporter.InitialDateLastActiveState",
                                  ToState(date_last_active));
    if (date_last_active == update_client::kDateUnknown) {
      // The pref store's scheduled write, or `OnCheckForUpdateComplete()` if it
      // runs first, commits this write. If the write is lost before then, the
      // next launch finds the date unknown and sends the same first-time ping.
      data->SetDateLastActive(app_id, update_client::kDateFirstTime);
    }
  }

  // Records the state of the ping freshness at `now` and, unless its write time
  // is unknown or in the future, its age. Then, if
  // `features::kActivityReporterPingFreshnessRotation` is enabled, replaces a
  // ping freshness that is expired, or whose write time is unknown or in the
  // future, and commits the prefs.
  void RecordAndRotatePingFreshness(base::Time now) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    update_client::PersistedData& data = *configurator_->GetPersistedData();
    const std::string id(kChromeActivityId);
    const update_client::PingFreshnessState state =
        update_client::GetPingFreshnessState(
            data, id, now, features::GetPingFreshnessRotationMaxAge());
    base::UmaHistogramEnumeration("ActivityReporter.PingFreshnessState", state);
    bool has_age = false;
    bool rotate = false;
    switch (state) {
      case update_client::PingFreshnessState::kNone:
        break;
      case update_client::PingFreshnessState::kFresh:
        has_age = true;
        break;
      case update_client::PingFreshnessState::kExpired:
        has_age = true;
        rotate = true;
        break;
      case update_client::PingFreshnessState::kUnknownWriteTime:
      case update_client::PingFreshnessState::kFutureWriteTime:
        rotate = true;
        break;
    }
    if (has_age) {
      const base::TimeDelta age = now - data.GetPingFreshnessWriteTime(id);
      base::UmaHistogramCustomCounts("ActivityReporter.PingFreshnessAge",
                                     age.InHours(), 1,
                                     kPingFreshnessAgeExclusiveMax.InHours(),
                                     kPingFreshnessAgeBucketCount);
    }
    if (!rotate || !base::FeatureList::IsEnabled(
                       features::kActivityReporterPingFreshnessRotation)) {
      return;
    }
    data.RotatePingFreshness(id);

    // Commit the new value now, so that the next session reuses it instead of
    // replacing it again if the browser exits before the update check
    // completes.
    if (PrefService* pref_service = configurator_->GetPrefService()) {
      pref_service->CommitPendingWrite();
    }
  }

  // Flushes the prefs written by the update check (even on error) so they
  // survive an early browser termination.
  void OnCheckForUpdateComplete(update_client::Error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_GT(incomplete_reports_, 0);
    --incomplete_reports_;
    if (PrefService* pref_service = configurator_->GetPrefService()) {
      pref_service->CommitPendingWrite();
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  std::optional<base::Time> last_reported_;
  int incomplete_reports_ = 0;
  const scoped_refptr<ActivityReporterConfigurator> configurator_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  base::RepeatingClosure updater_active_callback_;
  base::RepeatingCallback<version_info::Channel()> channel_provider_;
  base::RepeatingCallback<std::string()> brand_provider_;
  const raw_ptr<const base::Clock> clock_;
  base::WeakPtrFactory<ActivityReporterImpl> weak_factory_{this};
};

}  // namespace

std::unique_ptr<ActivityReporter> CreateActivityReporterImpl(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback,
    bool per_user_install) {
  return std::make_unique<ActivityReporterImpl>(
      std::move(pref_service_provider), std::move(network_fetcher_factory),
      std::move(channel_provider), std::move(brand_provider),
      std::move(updater_active_callback), per_user_install);
}

std::unique_ptr<ActivityReporter> CreateActivityReporterForTesting(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::UpdateClient> update_client,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback,
    const base::Clock* clock) {
  return std::make_unique<ActivityReporterImpl>(
      std::move(pref_service_provider), std::move(update_client),
      std::move(channel_provider), std::move(brand_provider),
      std::move(updater_active_callback), clock);
}

}  // namespace activity_reporter
