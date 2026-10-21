// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/activity_reporter/buildflags.h"
#include "components/activity_reporter/configurator.h"
#include "components/activity_reporter/constants.h"
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

class ActivityReporterImpl : public ActivityReporter {
 public:
  ActivityReporterImpl(
      base::RepeatingCallback<PrefService*()> pref_service_provider,
      scoped_refptr<update_client::NetworkFetcherFactory>
          network_fetcher_factory,
      base::RepeatingCallback<version_info::Channel()> channel_provider,
      base::RepeatingCallback<std::string()> brand_provider,
      base::RepeatingClosure updater_active_callback,
      bool per_user_install)
      : pref_service_provider_(pref_service_provider),
        updater_active_callback_(updater_active_callback),
        channel_provider_(channel_provider),
        brand_provider_(brand_provider) {
    scoped_refptr<ActivityReporterConfigurator> configurator =
        base::MakeRefCounted<ActivityReporterConfigurator>(
            pref_service_provider, network_fetcher_factory, channel_provider,
            per_user_install);
    update_client::PersistedData* data = configurator->GetPersistedData();
    if (data) {
      const int dla = data->GetDateLastActive(std::string(kChromeActivityId));
      base::UmaHistogramEnumeration(
          "ActivityReporter.InitialDateLastActiveState", ToState(dla));
      if (dla == update_client::kDateUnknown) {
        // Unlike the daynum written after an update check, this write is not
        // explicitly committed: if it is lost, the next launch re-derives it
        // from the unknown state and sends the same first-time ping.
        data->SetDateLastActive(std::string(kChromeActivityId),
                                update_client::kDateFirstTime);
      }
    }
    update_client_ = update_client::UpdateClientFactory(configurator);
  }

  ActivityReporterImpl(
      base::RepeatingCallback<PrefService*()> pref_service_provider,
      scoped_refptr<update_client::UpdateClient> update_client,
      base::RepeatingCallback<version_info::Channel()> channel_provider,
      base::RepeatingCallback<std::string()> brand_provider,
      base::RepeatingClosure updater_active_callback)
      : pref_service_provider_(pref_service_provider),
        update_client_(update_client),
        updater_active_callback_(updater_active_callback),
        channel_provider_(channel_provider),
        brand_provider_(brand_provider) {}

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
    const base::Time now = base::Time::Now();
    if (last_reported_ && now - *last_reported_ < base::Hours(5)) {
      // The last report was too recent; don't send another.
      return;
    }
    last_reported_ = base::Time::Now();
#if BUILDFLAG(IS_WIN)
    // Recorded once per Chrome active ping sent, so that the samples are
    // weighted by active use.
    RecordVirtualMachineHistogramsAsync();
#endif
    update_client_->CheckForUpdate(
        std::string{kChromeActivityId},
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
                  update_client::CrxComponent component;
                  component.app_id = kChromeActivityId;
                  component.brand = brand;
                  component.channel = version_info::GetChannelString(channel);
                  component.updates_enabled = false;
                  component.version = version_info::GetVersion();
                  components.push_back(component);
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
  // Flushes the prefs written by the update check (even on error) so they
  // survive an early browser termination.
  void OnCheckForUpdateComplete(update_client::Error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (PrefService* pref_service = pref_service_provider_.Run()) {
      pref_service->CommitPendingWrite();
    }
  }

  SEQUENCE_CHECKER(sequence_checker_);
  std::optional<base::Time> last_reported_;
  base::RepeatingCallback<PrefService*()> pref_service_provider_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  base::RepeatingClosure updater_active_callback_;
  base::RepeatingCallback<version_info::Channel()> channel_provider_;
  base::RepeatingCallback<std::string()> brand_provider_;
  base::WeakPtrFactory<ActivityReporterImpl> weak_factory_{this};
};

}  // namespace

// Must be called on a SequencedTaskRunner.
std::unique_ptr<ActivityReporter> CreateActivityReporter(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback,
    bool per_user_install) {
  return std::make_unique<ActivityReporterImpl>(
      pref_service_provider, network_fetcher_factory, channel_provider,
      brand_provider, updater_active_callback, per_user_install);
}

std::unique_ptr<ActivityReporter> CreateActivityReporterForTesting(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::UpdateClient> update_client,
    base::RepeatingCallback<version_info::Channel()> channel_provider,
    base::RepeatingCallback<std::string()> brand_provider,
    base::RepeatingClosure updater_active_callback) {
  return std::make_unique<ActivityReporterImpl>(
      pref_service_provider, update_client, channel_provider, brand_provider,
      updater_active_callback);
}

}  // namespace activity_reporter
