// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/service/sync_internals_util.h"

#include <string>
#include <utility>
#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/sync/base/time.h"
#include "components/sync/engine/cycle/sync_cycle_snapshot.h"
#include "components/sync/engine/sync_status.h"
#include "components/sync/engine/sync_string_conversions.h"
#include "components/sync/protocol/proto_enum_conversions.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_token_status.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/service/trusted_vault_synthetic_field_trial.h"
#include "components/version_info/version_info.h"
#include "url/gurl.h"

namespace syncer::sync_ui_util {

namespace {

const char kUninitialized[] = "Uninitialized";

const char kUninitializedCSSClass[] = "uninitialized";
const char kBadStateCSSClass[] = "in_bad_state";

std::string SeverityToString(TypeStatusForDebugging::Severity severity) {
  switch (severity) {
    case TypeStatusForDebugging::Severity::kError:
      return "severity_error";
    case TypeStatusForDebugging::Severity::kWarning:
      return "severity_warning";
    case TypeStatusForDebugging::Severity::kInfo:
      return "severity_info";
    case TypeStatusForDebugging::Severity::kTransitioning:
      return "transitioning";
    case TypeStatusForDebugging::Severity::kOk:
      return "ok";
  }
  NOTREACHED_NORETURN();
}

// Converts TypeStatusMapForDebugging to a base::Value::List.
base::Value::List TypeStatusMapToValueList(
    const TypeStatusMapForDebugging& map) {
  base::Value::List result;
  auto type_status_header = base::Value::Dict()
                                .Set("status", "header")
                                .Set("name", "Data Type")
                                .Set("num_entries", "Total Entries")
                                .Set("num_live", "Live Entries")
                                .Set("message", "Message")
                                .Set("state", "State");
  result.Append(std::move(type_status_header));
  for (const auto& [type, status] : map) {
    base::Value::Dict type_status;
    type_status.Set("name", DataTypeToDebugString(type));
    type_status.Set("status", SeverityToString(status.severity));
    type_status.Set("state", status.state);
    type_status.Set("message", status.message);
    result.Append(std::move(type_status));
  }
  return result;
}

// This class represents one field in chrome://sync-internals. It gets
// serialized into a dictionary with entries for 'stat_name', 'stat_value' and
// 'stat_status'.
class StatBase {
 public:
  base::Value::Dict ToValue() const {
    return base::Value::Dict()
        .Set("stat_name", base::Value(key_))
        .Set("stat_value", value_.Clone())
        .Set("stat_status", base::Value(status_));
  }

 protected:
  StatBase(const std::string& key, base::Value default_value)
      : key_(key), value_(std::move(default_value)) {}

  void SetFromValue(base::Value value, bool is_good) {
    value_ = std::move(value);
    status_ = is_good ? "" : kBadStateCSSClass;
  }

 private:
  std::string key_;
  base::Value value_;
  std::string status_ = kUninitializedCSSClass;
};

template <typename T>
class Stat : public StatBase {
 public:
  Stat(const std::string& key, const T& default_value)
      : StatBase(key, base::Value(default_value)) {}

  void Set(const T& value, bool is_good = true) {
    SetFromValue(base::Value(value), is_good);
  }
};

// A section for display on chrome://sync-internals, consisting of a title and a
// list of fields.
class Section {
 public:
  Section(const std::string& title, bool is_sensitive)
      : title_(title), is_sensitive_(is_sensitive) {}

  Stat<bool>* AddBoolStat(const std::string& key) {
    return AddStat(key, false);
  }
  Stat<int>* AddIntStat(const std::string& key) { return AddStat(key, 0); }
  Stat<std::string>* AddStringStat(const std::string& key) {
    return AddStat(key, std::string(kUninitialized));
  }

  base::Value::Dict ToValue() const {
    base::Value::List stats;
    for (const std::unique_ptr<StatBase>& stat : stats_) {
      stats.Append(stat->ToValue());
    }
    return base::Value::Dict()
        .Set("title", base::Value(title_))
        .Set("data", std::move(stats))
        .Set("is_sensitive", base::Value(is_sensitive_));
  }

  bool is_sensitive() { return is_sensitive_; }

 private:
  template <typename T>
  Stat<T>* AddStat(const std::string& key, const T& default_value) {
    auto stat = std::make_unique<Stat<T>>(key, default_value);
    Stat<T>* result = stat.get();
    stats_.push_back(std::move(stat));
    return result;
  }

  std::string title_;
  std::vector<std::unique_ptr<StatBase>> stats_;
  bool is_sensitive_ = false;
};

class SectionList {
 public:
  SectionList() = default;

  // WARNING: If this section includes any Personally Identifiable Information,
  // |is_sensitive| should be set to true.
  Section* AddSection(const std::string& title, bool is_sensitive) {
    sections_.push_back(std::make_unique<Section>(title, is_sensitive));
    return sections_.back().get();
  }

  // If |include_sensitive_data| is true, returns all added sections. Otherwise,
  // omits those added with |is_sensitive| set to true.
  base::Value::List ToValue(IncludeSensitiveData include_sensitive_data) const {
    base::Value::List result;
    for (const std::unique_ptr<Section>& section : sections_) {
      if (include_sensitive_data || !section->is_sensitive()) {
        result.Append(section->ToValue());
      }
    }
    return result;
  }

 private:
  std::vector<std::unique_ptr<Section>> sections_;
};

std::string GetDisableReasonsString(
    SyncService::DisableReasonSet disable_reasons) {
  if (disable_reasons.empty()) {
    return "None";
  }
  std::vector<std::string> reason_strings;
  if (disable_reasons.Has(SyncService::DISABLE_REASON_ENTERPRISE_POLICY)) {
    reason_strings.push_back("Enterprise policy");
  }
  if (disable_reasons.Has(SyncService::DISABLE_REASON_NOT_SIGNED_IN)) {
    reason_strings.push_back("Not signed in");
  }
  if (disable_reasons.Has(SyncService::DISABLE_REASON_UNRECOVERABLE_ERROR)) {
    reason_strings.push_back("Unrecoverable error");
  }
  return base::JoinString(reason_strings, ", ");
}

std::string GetTransportStateString(syncer::SyncService::TransportState state) {
  switch (state) {
    case syncer::SyncService::TransportState::DISABLED:
      return "Disabled";
    case syncer::SyncService::TransportState::PAUSED:
      return "Paused";
    case syncer::SyncService::TransportState::START_DEFERRED:
      return "Start deferred";
    case syncer::SyncService::TransportState::INITIALIZING:
      return "Initializing";
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
      return "Pending desired configuration";
    case syncer::SyncService::TransportState::CONFIGURING:
      return "Configuring data types";
    case syncer::SyncService::TransportState::ACTIVE:
      return "Active";
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

std::string GetUserActionableErrorString(
    SyncService::UserActionableError state) {
  switch (state) {
    case SyncService::UserActionableError::kNone:
      return "None";
    case SyncService::UserActionableError::kSignInNeedsUpdate:
      return "Sign-in needs update";
    case SyncService::UserActionableError::kNeedsPassphrase:
      return "Needs passphrase";
    case SyncService::UserActionableError::kNeedsTrustedVaultKeyForPasswords:
      return "Needs trusted vault key for passwords";
    case SyncService::UserActionableError::kNeedsTrustedVaultKeyForEverything:
      return "Needs trusted vault key for everything";
    case SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForPasswords:
      return "Trusted vault recoverability degraded for passwords";
    case SyncService::UserActionableError::
        kTrustedVaultRecoverabilityDegradedForEverything:
      return "Trusted vault recoverability degraded for everything";
  }

  NOTREACHED_IN_MIGRATION();
  return std::string();
}

// Returns a string describing the chrome version environment. Version format:
// <Build Info> <OS> <Version number> (<Last change>)<channel or "-devel">
// If version information is unavailable, returns "invalid."
// TODO(zea): this approximately matches syncer::MakeUserAgentForSync in
// sync_util.h. Unify the two if possible.
std::string GetVersionString(const std::string& channel) {
  // Build a version string that matches syncer::MakeUserAgentForSync with the
  // addition of channel info and proper OS names.
  // |channel| will be an empty string for stable channel or unofficial builds,
  // the channel string otherwise. We want to have "-devel" for unofficial
  // builds only.
  std::string version_modifier = channel;
  if (version_modifier.empty()) {
    if (!version_info::IsOfficialBuild()) {
      version_modifier = "-devel";
    }
  } else {
    version_modifier = " " + version_modifier;
  }
  return base::StrCat({version_info::GetProductName(), " ",
                       version_info::GetOSType(), " ",
                       version_info::GetVersionNumber(), " (",
                       version_info::GetLastChange(), ")", version_modifier});
}

std::string GetTimeStr(base::Time time,
                       const std::string& default_msg = "n/a") {
  if (time.is_null()) {
    return default_msg;
  }
  return GetTimeDebugString(time);
}

std::string GetTimeStrFromProto(int64_t proto_time,
                                const std::string& default_msg = "n/a") {
  if (proto_time == 0) {
    return default_msg;
  }
  return GetTimeDebugString(ProtoTimeToTime(proto_time));
}

// Analogous to GetTimeDebugString from components/sync/base/time.h. Consider
// moving it there if more places need this.
std::string GetTimeDeltaDebugString(base::TimeDelta t) {
  std::u16string result;
  if (!base::TimeDurationFormat(t, base::DURATION_WIDTH_NUMERIC, &result)) {
    return "Invalid TimeDelta?!";
  }
  return base::UTF16ToUTF8(result);
}

std::string GetLastSyncedTimeString(base::Time last_synced_time) {
  if (last_synced_time.is_null()) {
    return "Never";
  }

  base::TimeDelta time_since_last_sync = base::Time::Now() - last_synced_time;

  if (time_since_last_sync < base::Minutes(1)) {
    return "Just now";
  }

  return GetTimeDeltaDebugString(time_since_last_sync) + " ago";
}

std::string GetConnectionStatus(const SyncTokenStatus& status) {
  switch (status.connection_status) {
    case CONNECTION_NOT_ATTEMPTED:
      return "not attempted";
    case CONNECTION_OK:
      return base::StringPrintf(
          "OK since %s",
          GetTimeStr(status.connection_status_update_time).c_str());
    case CONNECTION_AUTH_ERROR:
      return base::StringPrintf(
          "auth error since %s",
          GetTimeStr(status.connection_status_update_time).c_str());
    case CONNECTION_SERVER_ERROR:
      return base::StringPrintf(
          "server error since %s",
          GetTimeStr(status.connection_status_update_time).c_str());
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace

// This function both defines the structure of the message to be returned and
// its contents.  Most of the message consists of simple fields in
// chrome://sync-internals which are grouped into sections and populated with
// the help of the SyncStat classes defined above.
base::Value::Dict ConstructAboutInformation(
    IncludeSensitiveData include_sensitive_data,
    SyncService* service,
    const std::string& channel) {
  base::Value::Dict about_info;

  SectionList section_list;

  Section* section_summary =
      section_list.AddSection("Summary", /*is_sensitive=*/false);
  Stat<std::string>* transport_state =
      section_summary->AddStringStat("Transport State");
  Stat<std::string>* error_state =
      section_summary->AddStringStat("User Actionable Error");
  Stat<std::string>* disable_reasons =
      section_summary->AddStringStat("Disable Reasons");
  Stat<bool>* feature_enabled =
      section_summary->AddBoolStat("Sync Feature Enabled");
  Stat<bool>* setup_in_progress =
      section_summary->AddBoolStat("Setup In Progress");
  Stat<std::string>* auth_error = section_summary->AddStringStat("Auth Error");

  Section* section_version =
      section_list.AddSection("Version Info", /*is_sensitive=*/false);
  Stat<std::string>* client_version =
      section_version->AddStringStat("Client Version");
  Stat<std::string>* server_url = section_version->AddStringStat("Server URL");

  Section* section_identity =
      section_list.AddSection(kIdentityTitle, /*is_sensitive=*/true);
  Stat<std::string>* sync_client_id =
      section_identity->AddStringStat("Sync Client ID");
  Stat<std::string>* username = section_identity->AddStringStat("Username");
  Stat<bool>* user_has_consent = section_identity->AddBoolStat("Sync Consent");

  Section* section_credentials =
      section_list.AddSection("Credentials", /*is_sensitive=*/false);
  Stat<std::string>* token_request_time =
      section_credentials->AddStringStat("Requested Token");
  Stat<std::string>* token_response_time =
      section_credentials->AddStringStat("Received Token Response");
  Stat<std::string>* last_token_request_result =
      section_credentials->AddStringStat("Last Token Request Result");
  Stat<bool>* has_token = section_credentials->AddBoolStat("Has Token");
  Stat<std::string>* next_token_request =
      section_credentials->AddStringStat("Next Token Request");

  Section* section_local =
      section_list.AddSection("Local State", /*is_sensitive=*/false);
  Stat<std::string>* server_connection =
      section_local->AddStringStat("Server Connection");
  Stat<std::string>* last_synced = section_local->AddStringStat("Last Synced");
  Stat<bool>* is_setup_complete =
      section_local->AddBoolStat("Sync First-Time Setup Complete");
  Stat<bool>* is_syncing = section_local->AddBoolStat("Sync Cycle Ongoing");
  Stat<bool>* is_local_sync_enabled =
      section_local->AddBoolStat("Local Sync Backend Enabled");
  Stat<std::string>* local_backend_path =
      section_local->AddStringStat("Local Backend Path");

  Section* section_network =
      section_list.AddSection("Network", /*is_sensitive=*/false);
  Stat<bool>* is_any_throttled_or_backoff =
      section_network->AddBoolStat("Throttled or Backoff");
  Stat<std::string>* retry_time = section_network->AddStringStat("Retry Time");
  Stat<bool>* are_notifications_enabled =
      section_network->AddBoolStat("Notifications Enabled");

  Section* section_encryption =
      section_list.AddSection("Encryption", /*is_sensitive=*/false);
  Stat<bool>* is_using_explicit_passphrase =
      section_encryption->AddBoolStat("Explicit Passphrase");
  Stat<bool>* is_passphrase_required =
      section_encryption->AddBoolStat("Passphrase Required");
  Stat<bool>* cryptographer_can_encrypt =
      section_encryption->AddBoolStat("Cryptographer Ready To Encrypt");
  Stat<bool>* has_pending_keys =
      section_encryption->AddBoolStat("Cryptographer Has Pending Keys");
  Stat<std::string>* encrypted_types =
      section_encryption->AddStringStat("Encrypted Types");
  Stat<bool>* has_keystore_key =
      section_encryption->AddBoolStat("Has Keystore Key");
  Stat<std::string>* keystore_migration_time =
      section_encryption->AddStringStat("Keystore Migration Time");
  Stat<std::string>* passphrase_type =
      section_encryption->AddStringStat("Passphrase Type");
  Stat<std::string>* explicit_passphrase_time =
      section_encryption->AddStringStat("Explicit passphrase Time");
  Stat<std::string>* trusted_vault_migration_time =
      section_encryption->AddStringStat("Trusted Vault Migration Time");
  Stat<int>* trusted_vault_key_version =
      section_encryption->AddIntStat("Trusted Vault Version/Epoch");
  Stat<std::string>* trusted_vault_auto_upgrade_experiment_group =
      section_encryption->AddStringStat("Trusted Vault Auto Upgrade Group");

  Section* section_last_session = section_list.AddSection(
      "Status from Last Completed Session", /*is_sensitive=*/false);
  Stat<std::string>* session_source =
      section_last_session->AddStringStat("Sync Source");
  Stat<bool>* get_key_failed =
      section_last_session->AddBoolStat("GetKey Step Failed");
  Stat<std::string>* download_result =
      section_last_session->AddStringStat("Download Step Result");
  Stat<std::string>* commit_result =
      section_last_session->AddStringStat("Commit Step Result");

  Section* section_counters =
      section_list.AddSection("Running Totals", /*is_sensitive=*/false);
  Stat<int>* notifications_received =
      section_counters->AddIntStat("Notifications Received");
  Stat<int>* updates_received =
      section_counters->AddIntStat("Updates Downloaded");
  Stat<int>* tombstone_updates =
      section_counters->AddIntStat("Tombstone Updates");
  Stat<int>* successful_commits =
      section_counters->AddIntStat("Successful Commits");

  Section* section_this_cycle = section_list.AddSection(
      "Transient Counters (this cycle)", /*is_sensitive=*/false);
  Stat<int>* server_conflicts =
      section_this_cycle->AddIntStat("Server Conflicts");
  Stat<int>* committed_items =
      section_this_cycle->AddIntStat("Committed Items");

  Section* section_that_cycle = section_list.AddSection(
      "Transient Counters (last cycle of last completed session)",
      /*is_sensitive=*/false);
  Stat<int>* updates_downloaded =
      section_that_cycle->AddIntStat("Updates Downloaded");
  Stat<int>* committed_count =
      section_that_cycle->AddIntStat("Committed Count");

  // Populate all the fields we declared above.
  client_version->Set(GetVersionString(channel));

  if (!service) {
    transport_state->Set("Sync service does not exist");
    error_state->Set("Sync service does not exist");
    about_info.Set(kDetailsKey, section_list.ToValue(include_sensitive_data));
    return about_info;
  }

  // Summary.
  transport_state->Set(GetTransportStateString(service->GetTransportState()));
  const SyncService::UserActionableError user_actionable_error =
      service->GetUserActionableError();
  error_state->Set(GetUserActionableErrorString(user_actionable_error),
                   /*is_good=*/user_actionable_error ==
                       SyncService::UserActionableError::kNone);
  disable_reasons->Set(GetDisableReasonsString(service->GetDisableReasons()));
  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is deleted.
  // See ConsentLevel::kSync documentation for details.
  feature_enabled->Set(service->IsSyncFeatureEnabled());
  setup_in_progress->Set(service->IsSetupInProgress());
  std::string auth_error_str = service->GetAuthError().ToString();
  auth_error->Set(
      base::StringPrintf(
          "%s since %s",
          (auth_error_str.empty() ? "OK" : auth_error_str).c_str(),
          GetTimeStr(service->GetAuthErrorTime(), "browser startup").c_str()),
      /*is_good=*/auth_error_str.empty());

  SyncStatus full_status;
  bool is_status_valid =
      service->QueryDetailedSyncStatusForDebugging(&full_status);
  const SyncCycleSnapshot& snapshot =
      service->GetLastCycleSnapshotForDebugging();
  const SyncTokenStatus& token_status =
      service->GetSyncTokenStatusForDebugging();
  bool is_local_sync_enabled_state = service->IsLocalSyncEnabled();

  // Version Info.
  // |client_version| was already set above.
  if (!is_local_sync_enabled_state) {
    server_url->Set(service->GetSyncServiceUrlForDebugging().spec());
  }

  // Identity.
  if (is_status_valid && !full_status.cache_guid.empty()) {
    sync_client_id->Set(full_status.cache_guid);
  }
  if (!is_local_sync_enabled_state) {
    username->Set(service->GetAccountInfo().email);
    // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is
    // deleted. See ConsentLevel::kSync documentation for details.
    user_has_consent->Set(service->HasSyncConsent());
  }

  // Credentials.
  token_request_time->Set(GetTimeStr(token_status.token_request_time));
  token_response_time->Set(GetTimeStr(token_status.token_response_time));
  std::string err = token_status.last_get_token_error.error_message();
  last_token_request_result->Set(err.empty() ? "OK" : err,
                                 /*is_good=*/err.empty());
  has_token->Set(token_status.has_token);
  next_token_request->Set(
      GetTimeStr(token_status.next_token_request_time, "not scheduled"));

  // Local State.
  server_connection->Set(
      GetConnectionStatus(token_status),
      /*is_good=*/token_status.connection_status == CONNECTION_NOT_ATTEMPTED ||
          token_status.connection_status == CONNECTION_OK);
  last_synced->Set(
      GetLastSyncedTimeString(service->GetLastSyncedTimeForDebugging()));
  // TODO(crbug.com/40067058): Delete this when ConsentLevel::kSync is deleted.
  // See ConsentLevel::kSync documentation for details.
  is_setup_complete->Set(
      service->GetUserSettings()->IsInitialSyncFeatureSetupComplete());
  if (is_status_valid) {
    is_syncing->Set(full_status.syncing);
  }
  is_local_sync_enabled->Set(is_local_sync_enabled_state);
  if (is_local_sync_enabled_state && is_status_valid) {
    local_backend_path->Set(full_status.local_sync_folder);
  }

  // Network.
  if (snapshot.is_initialized()) {
    is_any_throttled_or_backoff->Set(snapshot.is_silenced());
  }
  if (is_status_valid) {
    retry_time->Set(GetTimeStr(full_status.retry_time,
                               "Scheduler is not in backoff or throttled"));
  }
  if (is_status_valid) {
    are_notifications_enabled->Set(
        full_status.notifications_enabled,
        /*is_good=*/full_status.notifications_enabled);
  }

  // Encryption.
  if (service->IsEngineInitialized()) {
    is_using_explicit_passphrase->Set(
        service->GetUserSettings()->IsUsingExplicitPassphrase());
    is_passphrase_required->Set(
        service->GetUserSettings()->IsPassphraseRequired());
    explicit_passphrase_time->Set(
        GetTimeStr(service->GetUserSettings()->GetExplicitPassphraseTime()));
  }
  if (is_status_valid) {
    cryptographer_can_encrypt->Set(full_status.cryptographer_can_encrypt);
    has_pending_keys->Set(full_status.crypto_has_pending_keys);
    encrypted_types->Set(DataTypeSetToDebugString(full_status.encrypted_types));
    has_keystore_key->Set(full_status.has_keystore_key);
    keystore_migration_time->Set(
        GetTimeStr(full_status.keystore_migration_time, "Not Migrated"));
    passphrase_type->Set(PassphraseTypeToString(full_status.passphrase_type));

    if (full_status.passphrase_type ==
        PassphraseType::kTrustedVaultPassphrase) {
      trusted_vault_migration_time->Set(GetTimeStrFromProto(
          full_status.trusted_vault_debug_info.migration_time()));
      trusted_vault_key_version->Set(
          full_status.trusted_vault_debug_info.key_version());
    }

    if (full_status.trusted_vault_debug_info
            .has_auto_upgrade_experiment_group()) {
      const TrustedVaultAutoUpgradeSyntheticFieldTrialGroup group =
          TrustedVaultAutoUpgradeSyntheticFieldTrialGroup::FromProto(
              full_status.trusted_vault_debug_info
                  .auto_upgrade_experiment_group());
      trusted_vault_auto_upgrade_experiment_group->Set(
          group.is_valid() ? group.name() : std::string("Invalid"));
    }
  }

  // Status from Last Completed Session.
  if (snapshot.is_initialized()) {
    if (snapshot.get_updates_origin() != sync_pb::SyncEnums::UNKNOWN_ORIGIN) {
      session_source->Set(ProtoEnumToString(snapshot.get_updates_origin()));
    }
    const bool get_key_failed_state =
        snapshot.model_neutral_state().last_get_key_failed;
    get_key_failed->Set(get_key_failed_state,
                        /*is_good=*/!get_key_failed_state);
    SyncerError download_result_err =
        snapshot.model_neutral_state().last_download_updates_result;
    download_result->Set(
        download_result_err.ToString(),
        /*is_good=*/download_result_err.type() == SyncerError::Type::kSuccess);
    SyncerError commit_result_err =
        snapshot.model_neutral_state().commit_result;
    commit_result->Set(
        commit_result_err.ToString(),
        /*is_good=*/commit_result_err.type() == SyncerError::Type::kSuccess);
  }

  // Running Totals.
  if (is_status_valid) {
    notifications_received->Set(full_status.notifications_received);
    updates_received->Set(full_status.updates_received);
    tombstone_updates->Set(full_status.tombstone_updates_received);
    successful_commits->Set(full_status.num_commits_total);
  }

  // Transient Counters (this cycle).
  if (is_status_valid) {
    server_conflicts->Set(full_status.server_conflicts);
    committed_items->Set(full_status.committed_count);
  }

  // Transient Counters (last cycle of last completed session).
  if (snapshot.is_initialized()) {
    updates_downloaded->Set(
        snapshot.model_neutral_state().num_updates_downloaded_total);
    committed_count->Set(snapshot.model_neutral_state().num_successful_commits);
  }

  // This list of sections belongs in the 'details' field of the returned
  // message.
  about_info.Set(kDetailsKey, section_list.ToValue(include_sensitive_data));

  // The values set from this point onwards do not belong in the
  // details list.

  // We don't need to check is_status_valid here.
  // full_status.sync_protocol_error is exported directly from the
  // SyncServiceImpl, even if the backend doesn't exist.
  const bool actionable_error_detected =
      full_status.sync_protocol_error.error_type != UNKNOWN_ERROR &&
      full_status.sync_protocol_error.error_type != SYNC_SUCCESS;

  about_info.Set("actionable_error_detected",
                 base::Value(actionable_error_detected));

  // NOTE: We won't bother showing any of the following values unless
  // actionable_error_detected is set.

  Stat<std::string> error_type("Error Type", kUninitialized);
  Stat<std::string> action("Action", kUninitialized);
  Stat<std::string> description("Error Description", kUninitialized);

  if (actionable_error_detected) {
    error_type.Set(
        GetSyncErrorTypeString(full_status.sync_protocol_error.error_type));
    action.Set(GetClientActionString(full_status.sync_protocol_error.action));
    description.Set(full_status.sync_protocol_error.error_description);
  }

  about_info.Set("actionable_error", base::Value::List()
                                         .Append(error_type.ToValue())
                                         .Append(action.ToValue())
                                         .Append(description.ToValue()));

  about_info.Set("unrecoverable_error_detected",
                 base::Value(service->HasUnrecoverableError()));

  if (service->HasUnrecoverableError()) {
    std::string unrecoverable_error_message =
        "Unrecoverable error detected at " +
        service->GetUnrecoverableErrorLocationForDebugging().ToString() + ": " +
        service->GetUnrecoverableErrorMessageForDebugging();
    about_info.Set("unrecoverable_error_message",
                   base::Value(unrecoverable_error_message));
  }

  about_info.Set("type_status", TypeStatusMapToValueList(
                                    service->GetTypeStatusMapForDebugging()));

  return about_info;
}

}  // namespace syncer::sync_ui_util
