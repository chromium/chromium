// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/webui/policy_status_provider.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/no_destructor.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/policy/core/browser/cloud/message_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/cloud/cloud_policy_refresh_scheduler.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// Formats the association state indicated by |data|. If |data| is NULL, the
// state is considered to be UNMANAGED.
std::u16string FormatAssociationState(const em::PolicyData* data) {
  if (data) {
    switch (data->state()) {
      case em::PolicyData::ACTIVE:
        return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_ACTIVE);
      case em::PolicyData::UNMANAGED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
      case em::PolicyData::DEPROVISIONED:
        return l10n_util::GetStringUTF16(
            IDS_POLICY_ASSOCIATION_STATE_DEPROVISIONED);
    }
    NOTREACHED_IN_MIGRATION() << "Unknown state " << data->state();
  }

  // Default to UNMANAGED for the case of missing policy or bad state enum.
  return l10n_util::GetStringUTF16(IDS_POLICY_ASSOCIATION_STATE_UNMANAGED);
}

base::Clock* clock_for_testing_ = nullptr;

const base::Clock* GetClock() {
  if (clock_for_testing_)
    return clock_for_testing_;
  return base::DefaultClock::GetInstance();
}

}  // namespace

PolicyStatusProvider::PolicyStatusProvider() = default;

PolicyStatusProvider::~PolicyStatusProvider() = default;

void PolicyStatusProvider::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PolicyStatusProvider::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::Value::Dict PolicyStatusProvider::GetStatus() {
  // This method is called when the client is not enrolled.
  // Thus return an empty dictionary.
  return base::Value::Dict();
}

void PolicyStatusProvider::NotifyStatusChange() {
  for (auto& observer : observers_)
    observer.OnPolicyStatusChanged();
}

// static
base::Value::Dict PolicyStatusProvider::GetStatusFromCore(
    const CloudPolicyCore* core) {
  const CloudPolicyStore* store = core->store();
  const CloudPolicyClient* client = core->client();
  const CloudPolicyRefreshScheduler* refresh_scheduler =
      core->refresh_scheduler();

  const std::u16string status = GetPolicyStatusFromStore(store, client);

  const em::PolicyData* policy = store->policy();
  base::Value::Dict dict = GetStatusFromPolicyData(policy);

  base::TimeDelta refresh_interval = base::Milliseconds(
      refresh_scheduler ? refresh_scheduler->GetActualRefreshDelay()
                        : CloudPolicyRefreshScheduler::kDefaultRefreshDelayMs);

  const bool is_push_available =
      refresh_scheduler && refresh_scheduler->invalidations_available();

  bool no_error = store->status() == CloudPolicyStore::STATUS_OK && client &&
                  client->last_dm_status() == DM_STATUS_SUCCESS;
  dict.Set("error", !no_error);
  dict.Set("policiesPushAvailable", is_push_available);
  dict.Set("status", status);
  // If push is on, policy update will be done via push. Hide policy fetch
  // interval label to prevent users from misunderstanding.
  if (!is_push_available) {
    dict.Set(
        "refreshInterval",
        ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                               ui::TimeFormat::LENGTH_SHORT, refresh_interval));
  }
  base::Time last_refresh_time =
      policy && policy->has_timestamp()
          ? base::Time::FromMillisecondsSinceUnixEpoch(policy->timestamp())
          : base::Time();
  dict.Set("timeSinceLastRefresh",
           GetTimeSinceLastActionString(last_refresh_time));

  // In case state_keys aren't available, we have no scheduler. See also
  // DeviceCloudPolicyInitializer::TryToCreateClient and b/181140445.
  base::Time last_fetch_attempted_time =
      refresh_scheduler ? refresh_scheduler->last_refresh() : base::Time();
  dict.Set("timeSinceLastFetchAttempt",
           GetTimeSinceLastActionString(last_fetch_attempted_time));
  return dict;
}

// static
base::Value::Dict PolicyStatusProvider::GetStatusFromPolicyData(
    const em::PolicyData* policy) {
  base::Value::Dict dict;
  if (!policy) {
    dict.Set(kClientIdKey, std::string());
    dict.Set(kUsernameKey, std::string());
    return dict;
  }

  dict.Set(kClientIdKey, policy->device_id());
  dict.Set(kUsernameKey, policy->username());

  if (policy->has_annotated_asset_id()) {
    dict.Set(kAssetIdKey, policy->annotated_asset_id());
  }
  if (policy->has_annotated_location()) {
    dict.Set(kLocationKey, policy->annotated_location());
  }
  if (policy->has_directory_api_id()) {
    dict.Set(kDirectoryApiIdKey, policy->directory_api_id());
  }
  if (policy->has_gaia_id()) {
    dict.Set(kGaiaIdKey, policy->gaia_id());
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Include the "Managed by:" attribute for the user policy legend.
  if (policy->state() == enterprise_management::PolicyData::ACTIVE) {
    if (policy->has_managed_by())
      dict.Set(kEnterpriseDomainManagerKey, policy->managed_by());
    else if (policy->has_display_domain())
      dict.Set(kEnterpriseDomainManagerKey, policy->display_domain());
  }
#endif
  return dict;
}

// static
void PolicyStatusProvider::UpdateLastReportTimestamp(
    base::Value::Dict& status,
    PrefService* prefs,
    const std::string& report_timestamp_pref_path) {
  if (prefs->HasPrefPath(report_timestamp_pref_path)) {
    base::Time last_report_timestamp =
        prefs->GetTime(report_timestamp_pref_path);
    status.Set("lastCloudReportSentTimestamp",
               base::UnlocalizedTimeFormatWithPattern(last_report_timestamp,
                                                      "yyyy-LL-dd HH:mm zzz"));
    status.Set("timeSinceLastCloudReportSent",
               GetTimeSinceLastActionString(last_report_timestamp));
  }
}

// CloudPolicyStore errors take precedence to show in the status message.
// Other errors (such as transient policy fetching problems) get displayed
// only if CloudPolicyStore is in STATUS_OK.
// static
std::u16string PolicyStatusProvider::GetPolicyStatusFromStore(
    const CloudPolicyStore* store,
    const CloudPolicyClient* client) {
  if (store->status() == CloudPolicyStore::STATUS_OK) {
    if (client && client->last_dm_status() != DM_STATUS_SUCCESS)
      return FormatDeviceManagementStatus(client->last_dm_status());
    else if (!store->is_managed())
      return FormatAssociationState(store->policy());
  }

  return FormatStoreStatus(store->status(), store->validation_status());
}

// static
std::u16string PolicyStatusProvider::GetTimeSinceLastActionString(
    base::Time last_action_time) {
  if (last_action_time.is_null())
    return l10n_util::GetStringUTF16(IDS_POLICY_NEVER_FETCHED);
  base::Time now = GetClock()->Now();
  base::TimeDelta elapsed_time;
  if (now > last_action_time)
    elapsed_time = now - last_action_time;
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT, elapsed_time);
}

// static
base::ScopedClosureRunner PolicyStatusProvider::OverrideClockForTesting(
    base::Clock* clock_for_testing) {
  CHECK(!clock_for_testing_);
  clock_for_testing_ = clock_for_testing;
  return base::ScopedClosureRunner(
      base::BindOnce([]() { clock_for_testing_ = nullptr; }));
}

}  // namespace policy
