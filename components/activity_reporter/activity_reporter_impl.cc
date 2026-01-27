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
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version_info/version_info.h"
#include "components/activity_reporter/activity_reporter.h"
#include "components/activity_reporter/buildflags.h"
#include "components/activity_reporter/configurator.h"
#include "components/activity_reporter/constants.h"
#include "components/prefs/pref_service.h"
#include "components/update_client/network.h"
#include "components/update_client/update_client.h"

namespace activity_reporter {

namespace {

class ActivityReporterImpl : public ActivityReporter {
 public:
  ActivityReporterImpl(
      base::RepeatingCallback<PrefService*()> pref_service_provider,
      scoped_refptr<update_client::NetworkFetcherFactory>
          network_fetcher_factory,
      base::RepeatingClosure updater_active_callback)
      : update_client_(update_client::UpdateClientFactory(
            base::MakeRefCounted<ActivityReporterConfigurator>(
                pref_service_provider,
                network_fetcher_factory))),
        updater_active_callback_(updater_active_callback) {}

  ActivityReporterImpl(scoped_refptr<update_client::UpdateClient> update_client,
                       base::RepeatingClosure updater_active_callback)
      : update_client_(update_client),
        updater_active_callback_(updater_active_callback) {}

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
      // TODO(crbug.com/454662418): Consider scheduling an activity report.
      return;
    }
    last_reported_ = base::Time::Now();
    update_client_->CheckForUpdate(
        kChromeActivityId,
        base::BindOnce([](const std::vector<std::string>& ids,
                          base::OnceCallback<void(
                              const std::vector<std::optional<
                                  update_client::CrxComponent>>&)> callback) {
          std::vector<std::optional<update_client::CrxComponent>> components;
          for (const std::string& id : ids) {
            if (id == kChromeActivityId) {
              update_client::CrxComponent component;
              component.app_id = kChromeActivityId;
              // TODO(crbug.com/454662418): Set component.brand
              // TODO(crbug.com/454662418): Set component.channel
              component.updates_enabled = false;
              component.version = version_info::GetVersion();
              components.push_back(component);
            } else {
              components.push_back(std::nullopt);
            }
          }
          std::move(callback).Run(components);
        }),
        base::DoNothing(), false, base::DoNothing());
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  std::optional<base::Time> last_reported_;
  scoped_refptr<update_client::UpdateClient> update_client_;
  base::RepeatingClosure updater_active_callback_;
};

}  // namespace

// Must be called on a SequencedTaskRunner.
std::unique_ptr<ActivityReporter> CreateActivityReporter(
    base::RepeatingCallback<PrefService*()> pref_service_provider,
    scoped_refptr<update_client::NetworkFetcherFactory> network_fetcher_factory,
    base::RepeatingClosure updater_active_callback) {
  return std::make_unique<ActivityReporterImpl>(
      pref_service_provider, network_fetcher_factory, updater_active_callback);
}

std::unique_ptr<ActivityReporter> CreateActivityReporterForTesting(
    scoped_refptr<update_client::UpdateClient> update_client,
    base::RepeatingClosure updater_active_callback) {
  return std::make_unique<ActivityReporterImpl>(update_client,
                                                updater_active_callback);
}

}  // namespace activity_reporter
