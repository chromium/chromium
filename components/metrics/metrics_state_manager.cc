// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_state_manager.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/metrics/cloned_install_detector.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/machine_id_provider.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_provider.h"
#include "components/metrics/metrics_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace metrics {

namespace {

// The argument used to generate a non-identifying entropy source. We want no
// more than 13 bits of entropy, so use this max to return a number in the range
// [0, 7999] as the entropy source (12.97 bits of entropy).
const int kMaxLowEntropySize = 8000;

// Generates a new non-identifying entropy source used to seed persistent
// activities.
int GenerateLowEntropySource() {
  return base::RandInt(0, kMaxLowEntropySize - 1);
}

int64_t ReadEnabledDate(PrefService* local_state) {
  return local_state->GetInt64(prefs::kMetricsReportingEnabledTimestamp);
}

int64_t ReadInstallDate(PrefService* local_state) {
  return local_state->GetInt64(prefs::kInstallDate);
}

// Round a timestamp measured in seconds since epoch to one with a granularity
// of an hour. This can be used before uploaded potentially sensitive
// timestamps.
int64_t RoundSecondsToHour(int64_t time_in_seconds) {
  return 3600 * (time_in_seconds / 3600);
}

// Records the cloned install histogram.
void LogClonedInstall() {
  // Equivalent to UMA_HISTOGRAM_BOOLEAN with the stability flag set.
  UMA_STABILITY_HISTOGRAM_ENUMERATION("UMA.IsClonedInstall", 1, 2);
}

class MetricsStateMetricsProvider : public MetricsProvider {
 public:
  MetricsStateMetricsProvider(PrefService* local_state,
                              bool metrics_ids_were_reset,
                              std::string previous_client_id)
      : local_state_(local_state),
        metrics_ids_were_reset_(metrics_ids_were_reset),
        previous_client_id_(std::move(previous_client_id)) {}

  // MetricsProvider:
  void ProvideSystemProfileMetrics(
      SystemProfileProto* system_profile) override {
    system_profile->set_uma_enabled_date(
        RoundSecondsToHour(ReadEnabledDate(local_state_)));
    system_profile->set_install_date(
        RoundSecondsToHour(ReadInstallDate(local_state_)));
  }

  void ProvidePreviousSessionData(
      ChromeUserMetricsExtension* uma_proto) override {
    if (metrics_ids_were_reset_) {
      LogClonedInstall();
      if (!previous_client_id_.empty()) {
        // If we know the previous client id, overwrite the client id for the
        // previous session log so the log contains the client id at the time
        // of the previous session. This allows better attribution of crashes
        // to earlier behavior. If the previous client id is unknown, leave
        // the current client id.
        uma_proto->set_client_id(MetricsLog::Hash(previous_client_id_));
      }
    }
  }

  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override {
    if (local_state_->GetBoolean(prefs::kMetricsResetIds))
      LogClonedInstall();
  }

 private:
  PrefService* const local_state_;
  const bool metrics_ids_were_reset_;
  // |previous_client_id_| is set only (if known) when |metrics_ids_were_reset_|
  const std::string previous_client_id_;

  DISALLOW_COPY_AND_ASSIGN(MetricsStateMetricsProvider);
};

}  // namespace

// static
bool MetricsStateManager::instance_exists_ = false;

MetricsStateManager::MetricsStateManager(
    PrefService* local_state,
    EnabledStateProvider* enabled_state_provider,
    const base::string16& backup_registry_key,
    const StoreClientInfoCallback& store_client_info,
    const LoadClientInfoCallback& retrieve_client_info)
    : local_state_(local_state),
      enabled_state_provider_(enabled_state_provider),
      store_client_info_(store_client_info),
      load_client_info_(retrieve_client_info),
      clean_exit_beacon_(backup_registry_key, local_state),
      low_entropy_source_(kLowEntropySourceNotSet),
      old_low_entropy_source_(kLowEntropySourceNotSet),
      entropy_source_returned_(ENTROPY_SOURCE_NONE),
      metrics_ids_were_reset_(false) {
  ResetMetricsIDsIfNecessary();
  if (enabled_state_provider_->IsConsentGiven())
    ForceClientIdCreation();

  // Set the install date if this is our first run.
  int64_t install_date = local_state_->GetInt64(prefs::kInstallDate);
  if (install_date == 0) {
    local_state_->SetInt64(prefs::kInstallDate, base::Time::Now().ToTimeT());

#if !defined(OS_WIN)
    // If this is a first run (no install date) and there's no client id, then
    // generate a provisional client id now. This id will be used for field
    // trial randomization on first run and will be promoted to become the
    // client id if UMA is enabled during this session, via the logic in
    // ForceClientIdCreation().
    //
    // Note: We don't do this on Windows because on Windows, there's no UMA
    // checkbox on first run and instead it comes from the install page. So if
    // UMA is not enabled at this point, it's unlikely it will be enabled in
    // the same session since that requires the user to manually do that via
    // settings page after they unchecked it on the download page.
    //
    // Note: Windows first run is covered by browser tests
    // FirstRunMasterPrefsVariationsSeedTest.PRE_SecondRun and
    // FirstRunMasterPrefsVariationsSeedTest.SecondRun. If the platform ifdef
    // for this logic changes, the tests should be updated as well.
    if (client_id_.empty())
      provisional_client_id_ = base::GenerateGUID();
#endif  // !defined(OS_WIN)
  }

  DCHECK(!instance_exists_);
  instance_exists_ = true;
}

MetricsStateManager::~MetricsStateManager() {
  DCHECK(instance_exists_);
  instance_exists_ = false;
}

std::unique_ptr<MetricsProvider> MetricsStateManager::GetProvider() {
  return std::make_unique<MetricsStateMetricsProvider>(
      local_state_, metrics_ids_were_reset_, previous_client_id_);
}

bool MetricsStateManager::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

int64_t MetricsStateManager::GetInstallDate() const {
  return ReadInstallDate(local_state_);
}

void MetricsStateManager::ForceClientIdCreation() {
  // TODO(asvitkine): Ideally, all tests would actually set up consent properly,
  // so the command-line check wouldn't be needed here.
  // Currently, kForceEnableMetricsReporting is used by Java UkmTest and
  // kMetricsRecordingOnly is used by Chromedriver tests.
  DCHECK(enabled_state_provider_->IsConsentGiven() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kForceEnableMetricsReporting) ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kMetricsRecordingOnly));
  {
    std::string client_id_from_prefs =
        local_state_->GetString(prefs::kMetricsClientID);
    // If client id in prefs matches the cached copy, return early.
    if (!client_id_from_prefs.empty() && client_id_from_prefs == client_id_)
      return;
    client_id_.swap(client_id_from_prefs);
  }

  if (!client_id_.empty())
    return;

  const std::unique_ptr<ClientInfo> client_info_backup = LoadClientInfo();
  if (client_info_backup) {
    client_id_ = client_info_backup->client_id;

    const base::Time now = base::Time::Now();

    // Save the recovered client id and also try to reinstantiate the backup
    // values for the dates corresponding with that client id in order to avoid
    // weird scenarios where we could report an old client id with a recent
    // install date.
    local_state_->SetString(prefs::kMetricsClientID, client_id_);
    local_state_->SetInt64(prefs::kInstallDate,
                           client_info_backup->installation_date != 0
                               ? client_info_backup->installation_date
                               : now.ToTimeT());
    local_state_->SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                           client_info_backup->reporting_enabled_date != 0
                               ? client_info_backup->reporting_enabled_date
                               : now.ToTimeT());

    base::TimeDelta recovered_installation_age;
    if (client_info_backup->installation_date != 0) {
      recovered_installation_age =
          now - base::Time::FromTimeT(client_info_backup->installation_date);
    }
    UMA_HISTOGRAM_COUNTS_10000("UMA.ClientIdBackupRecoveredWithAge",
                               recovered_installation_age.InHours());

    // Flush the backup back to persistent storage in case we re-generated
    // missing data above.
    BackUpCurrentClientInfo();
    return;
  }

  // If we're here, there was no client ID yet (either in prefs or backup),
  // so generate a new one. If there's a provisional client id (e.g. UMA
  // was enabled as part of first run), promote that to the client id,
  // otherwise (e.g. UMA enabled in a future session), generate a new one.
  if (provisional_client_id_.empty()) {
    client_id_ = base::GenerateGUID();
  } else {
    client_id_ = provisional_client_id_;
    provisional_client_id_.clear();
  }
  local_state_->SetString(prefs::kMetricsClientID, client_id_);

  // Record the timestamp of when the user opted in to UMA.
  local_state_->SetInt64(prefs::kMetricsReportingEnabledTimestamp,
                         base::Time::Now().ToTimeT());

  BackUpCurrentClientInfo();
}

void MetricsStateManager::CheckForClonedInstall() {
  DCHECK(!cloned_install_detector_);

  if (!MachineIdProvider::HasId())
    return;

  cloned_install_detector_ = std::make_unique<ClonedInstallDetector>();
  cloned_install_detector_->CheckForClonedInstall(local_state_);
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
MetricsStateManager::CreateDefaultEntropyProvider() {
  if (enabled_state_provider_->IsConsentGiven() ||
      !provisional_client_id_.empty()) {
    UpdateEntropySourceReturnedValue(ENTROPY_SOURCE_HIGH);
    return std::make_unique<variations::SHA1EntropyProvider>(
        GetHighEntropySource());
  }

  UpdateEntropySourceReturnedValue(ENTROPY_SOURCE_LOW);
  return CreateLowEntropyProvider();
}

std::unique_ptr<const base::FieldTrial::EntropyProvider>
MetricsStateManager::CreateLowEntropyProvider() {
  int source = GetLowEntropySource();
  return std::make_unique<variations::NormalizedMurmurHashEntropyProvider>(
      base::checked_cast<uint16_t>(source), kMaxLowEntropySize);
}

// static
std::unique_ptr<MetricsStateManager> MetricsStateManager::Create(
    PrefService* local_state,
    EnabledStateProvider* enabled_state_provider,
    const base::string16& backup_registry_key,
    const StoreClientInfoCallback& store_client_info,
    const LoadClientInfoCallback& retrieve_client_info) {
  std::unique_ptr<MetricsStateManager> result;
  // Note: |instance_exists_| is updated in the constructor and destructor.
  if (!instance_exists_) {
    result.reset(new MetricsStateManager(local_state, enabled_state_provider,
                                         backup_registry_key, store_client_info,
                                         retrieve_client_info));
  }
  return result;
}

// static
void MetricsStateManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kMetricsResetIds, false);
  registry->RegisterStringPref(prefs::kMetricsClientID, std::string());
  registry->RegisterInt64Pref(prefs::kMetricsReportingEnabledTimestamp, 0);
  registry->RegisterIntegerPref(prefs::kMetricsLowEntropySource,
                                kLowEntropySourceNotSet);
  registry->RegisterIntegerPref(prefs::kMetricsOldLowEntropySource,
                                kLowEntropySourceNotSet);
  registry->RegisterInt64Pref(prefs::kInstallDate, 0);

  ClonedInstallDetector::RegisterPrefs(registry);
}

// static
constexpr int MetricsStateManager::kLowEntropySourceNotSet;

void MetricsStateManager::BackUpCurrentClientInfo() {
  ClientInfo client_info;
  client_info.client_id = client_id_;
  client_info.installation_date = ReadInstallDate(local_state_);
  client_info.reporting_enabled_date = ReadEnabledDate(local_state_);
  store_client_info_.Run(client_info);
}

std::unique_ptr<ClientInfo> MetricsStateManager::LoadClientInfo() {
  std::unique_ptr<ClientInfo> client_info = load_client_info_.Run();

  // The GUID retrieved should be valid unless retrieval failed.
  // If not, return nullptr. This will result in a new GUID being generated by
  // the calling function ForceClientIdCreation().
  if (client_info && !base::IsValidGUID(client_info->client_id))
    return nullptr;

  return client_info;
}

std::string MetricsStateManager::GetHighEntropySource() {
  // This should only be called if UMA is enabled or there's a provisional
  // client id. If UMA is enabled, then the constructor should have loaded
  // |client_id_|. The user shouldn't be able to enable UMA between the
  // constructor and calling this, because field trial setup happens at Chrome
  // initialization. Only one of these is expected to hold a value.
  DCHECK(client_id_.empty() != provisional_client_id_.empty());

  // For metrics reporting-enabled users, we combine the client ID and low
  // entropy source to get the final entropy source.
  // This has two useful properties:
  //  1) It makes the entropy source less identifiable for parties that do not
  //     know the low entropy source.
  //  2) It makes the final entropy source resettable.

  // If this install has an old low entropy source, continue using it, to avoid
  // changing the group assignments of studies using high entropy. New installs
  // only have the new low entropy source. If the number of installs with old
  // sources ever becomes small enough (see UMA.LowEntropySourceValue), we could
  // remove it, and just use the new source here.
  int low_entropy_source = GetOldLowEntropySource();
  if (low_entropy_source == kLowEntropySourceNotSet)
    low_entropy_source = GetLowEntropySource();

  const std::string& client_id_to_use =
      (client_id_.empty() ? provisional_client_id_ : client_id_);
  return client_id_to_use + base::NumberToString(low_entropy_source);
}

int MetricsStateManager::GetLowEntropySource() {
  UpdateLowEntropySources();
  return low_entropy_source_;
}

int MetricsStateManager::GetOldLowEntropySource() {
  UpdateLowEntropySources();
  return old_low_entropy_source_;
}

void MetricsStateManager::UpdateLowEntropySources() {
  // The default value for |low_entropy_source_| and the default pref value are
  // both |kLowEntropySourceNotSet|, which indicates the value has not been set.
  if (low_entropy_source_ != kLowEntropySourceNotSet)
    return;

  const base::CommandLine* command_line(base::CommandLine::ForCurrentProcess());
  // Only try to load the value from prefs if the user did not request a reset.
  // Otherwise, skip to generating a new value. We would have already returned
  // if |low_entropy_source_| were set, ensuring we only do this reset on the
  // first call to UpdateLowEntropySources().
  if (!command_line->HasSwitch(switches::kResetVariationState)) {
    int new_pref = local_state_->GetInteger(prefs::kMetricsLowEntropySource);
    if (IsValidLowEntropySource(new_pref))
      low_entropy_source_ = new_pref;
    int old_pref = local_state_->GetInteger(prefs::kMetricsOldLowEntropySource);
    if (IsValidLowEntropySource(old_pref))
      old_low_entropy_source_ = old_pref;
  }

  // If the new source is missing or corrupt (or requested to be reset), then
  // (re)create it. Don't bother recreating the old source if it's corrupt,
  // because we only keep the old source around for consistency, and we can't
  // maintain a consistent value if we recreate it.
  if (low_entropy_source_ == kLowEntropySourceNotSet) {
    low_entropy_source_ = GenerateLowEntropySource();
    DCHECK(IsValidLowEntropySource(low_entropy_source_));
    local_state_->SetInteger(prefs::kMetricsLowEntropySource,
                             low_entropy_source_);
  }

  // If the old source was present but corrupt (or requested to be reset), then
  // we'll never use it again, so delete it.
  if (old_low_entropy_source_ == kLowEntropySourceNotSet &&
      local_state_->HasPrefPath(prefs::kMetricsOldLowEntropySource)) {
    local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
  }

  DCHECK_NE(low_entropy_source_, kLowEntropySourceNotSet);
  base::UmaHistogramSparse("UMA.LowEntropySource3Value", low_entropy_source_);
  if (old_low_entropy_source_ != kLowEntropySourceNotSet) {
    base::UmaHistogramSparse("UMA.LowEntropySourceValue",
                             old_low_entropy_source_);
  }
}

void MetricsStateManager::UpdateEntropySourceReturnedValue(
    EntropySourceType type) {
  if (entropy_source_returned_ != ENTROPY_SOURCE_NONE)
    return;

  entropy_source_returned_ = type;
  UMA_HISTOGRAM_ENUMERATION("UMA.EntropySourceType", type,
                            ENTROPY_SOURCE_ENUM_SIZE);
}

void MetricsStateManager::ResetMetricsIDsIfNecessary() {
  if (!local_state_->GetBoolean(prefs::kMetricsResetIds))
    return;
  metrics_ids_were_reset_ = true;
  previous_client_id_ = local_state_->GetString(prefs::kMetricsClientID);

  UMA_HISTOGRAM_BOOLEAN("UMA.MetricsIDsReset", true);

  DCHECK(client_id_.empty());
  DCHECK_EQ(kLowEntropySourceNotSet, low_entropy_source_);

  local_state_->ClearPref(prefs::kMetricsClientID);
  local_state_->ClearPref(prefs::kMetricsLowEntropySource);
  local_state_->ClearPref(prefs::kMetricsOldLowEntropySource);
  local_state_->ClearPref(prefs::kMetricsResetIds);

  // Also clear the backed up client info.
  store_client_info_.Run(ClientInfo());
}

// static
bool MetricsStateManager::IsValidLowEntropySource(int value) {
  return value >= 0 && value < kMaxLowEntropySize;
}

}  // namespace metrics
