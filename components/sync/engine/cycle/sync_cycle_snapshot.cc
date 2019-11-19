// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

#include <utility>

#include "base/base64.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "components/sync/protocol/proto_enum_conversions.h"

namespace syncer {

namespace {

base::string16 FormatTimeDelta(base::TimeDelta delta) {
  base::string16 value;
  bool ok =
      base::TimeDurationFormat(delta, base::DURATION_WIDTH_NARROW, &value);
  DCHECK(ok);
  return value;
}

}  // namespace

SyncCycleSnapshot::SyncCycleSnapshot()
    : is_silenced_(false),
      num_encryption_conflicts_(0),
      num_hierarchy_conflicts_(0),
      num_server_conflicts_(0),
      notifications_enabled_(false),
      num_entries_(0),
      num_entries_by_type_(ModelType::NUM_ENTRIES, 0),
      num_to_delete_entries_by_type_(ModelType::NUM_ENTRIES, 0),
      has_remaining_local_changes_(false),
      is_initialized_(false) {}

SyncCycleSnapshot::SyncCycleSnapshot(
    const std::string& birthday,
    const std::string& bag_of_chips,
    const ModelNeutralState& model_neutral_state,
    const ProgressMarkerMap& download_progress_markers,
    bool is_silenced,
    int num_encryption_conflicts,
    int num_hierarchy_conflicts,
    int num_server_conflicts,
    bool notifications_enabled,
    size_t num_entries,
    base::Time sync_start_time,
    base::Time poll_finish_time,
    const std::vector<int>& num_entries_by_type,
    const std::vector<int>& num_to_delete_entries_by_type,
    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin,
    base::TimeDelta poll_interval,
    bool has_remaining_local_changes)
    : birthday_(birthday),
      bag_of_chips_(bag_of_chips),
      model_neutral_state_(model_neutral_state),
      download_progress_markers_(download_progress_markers),
      is_silenced_(is_silenced),
      num_encryption_conflicts_(num_encryption_conflicts),
      num_hierarchy_conflicts_(num_hierarchy_conflicts),
      num_server_conflicts_(num_server_conflicts),
      notifications_enabled_(notifications_enabled),
      num_entries_(num_entries),
      sync_start_time_(sync_start_time),
      poll_finish_time_(poll_finish_time),
      num_entries_by_type_(num_entries_by_type),
      num_to_delete_entries_by_type_(num_to_delete_entries_by_type),
      get_updates_origin_(get_updates_origin),
      poll_interval_(poll_interval),
      has_remaining_local_changes_(has_remaining_local_changes),
      is_initialized_(true) {}

SyncCycleSnapshot::SyncCycleSnapshot(const SyncCycleSnapshot& other) = default;

SyncCycleSnapshot::~SyncCycleSnapshot() {}

std::unique_ptr<base::DictionaryValue> SyncCycleSnapshot::ToValue() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString("birthday", birthday_);
  std::string encoded_bag_of_chips;
  base::Base64Encode(bag_of_chips_, &encoded_bag_of_chips);
  value->SetString("bagOfChips", encoded_bag_of_chips);
  value->SetInteger("numSuccessfulCommits",
                    model_neutral_state_.num_successful_commits);
  value->SetInteger("numSuccessfulBookmarkCommits",
                    model_neutral_state_.num_successful_bookmark_commits);
  value->SetInteger("numUpdatesDownloadedTotal",
                    model_neutral_state_.num_updates_downloaded_total);
  value->SetInteger(
      "numTombstoneUpdatesDownloadedTotal",
      model_neutral_state_.num_tombstone_updates_downloaded_total);
  value->SetInteger(
      "numReflectedUpdatesDownloadedTotal",
      model_neutral_state_.num_reflected_updates_downloaded_total);
  value->SetInteger("numLocalOverwrites",
                    model_neutral_state_.num_local_overwrites);
  value->SetInteger("numServerOverwrites",
                    model_neutral_state_.num_server_overwrites);
  value->Set("downloadProgressMarkers",
             ProgressMarkerMapToValue(download_progress_markers_));
  value->SetBoolean("isSilenced", is_silenced_);
  // We don't care too much if we lose precision here, also.
  value->SetInteger("numEncryptionConflicts", num_encryption_conflicts_);
  value->SetInteger("numHierarchyConflicts", num_hierarchy_conflicts_);
  value->SetInteger("numServerConflicts", num_server_conflicts_);
  value->SetInteger("numEntries", num_entries_);
  value->SetString("getUpdatesOrigin", ProtoEnumToString(get_updates_origin_));
  value->SetBoolean("notificationsEnabled", notifications_enabled_);

  std::unique_ptr<base::DictionaryValue> counter_entries(
      new base::DictionaryValue());
  for (int i = FIRST_REAL_MODEL_TYPE; i < ModelType::NUM_ENTRIES; i++) {
    std::unique_ptr<base::DictionaryValue> type_entries(
        new base::DictionaryValue());
    type_entries->SetInteger("numEntries", num_entries_by_type_[i]);
    type_entries->SetInteger("numToDeleteEntries",
                             num_to_delete_entries_by_type_[i]);

    const std::string model_type = ModelTypeToString(static_cast<ModelType>(i));
    counter_entries->Set(model_type, std::move(type_entries));
  }
  value->Set("counter_entries", std::move(counter_entries));
  value->SetBoolean("hasRemainingLocalChanges", has_remaining_local_changes_);
  value->SetString("poll_interval", FormatTimeDelta(poll_interval_));
  value->SetString(
      "poll_finish_time",
      base::TimeFormatShortDateAndTimeWithTimeZone(poll_finish_time_));
  return value;
}

std::string SyncCycleSnapshot::ToString() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *ToValue(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

const ProgressMarkerMap& SyncCycleSnapshot::download_progress_markers() const {
  return download_progress_markers_;
}

bool SyncCycleSnapshot::is_silenced() const {
  return is_silenced_;
}

int SyncCycleSnapshot::num_encryption_conflicts() const {
  return num_encryption_conflicts_;
}

int SyncCycleSnapshot::num_hierarchy_conflicts() const {
  return num_hierarchy_conflicts_;
}

int SyncCycleSnapshot::num_server_conflicts() const {
  return num_server_conflicts_;
}

bool SyncCycleSnapshot::notifications_enabled() const {
  return notifications_enabled_;
}

size_t SyncCycleSnapshot::num_entries() const {
  return num_entries_;
}

base::Time SyncCycleSnapshot::sync_start_time() const {
  return sync_start_time_;
}

base::Time SyncCycleSnapshot::poll_finish_time() const {
  return poll_finish_time_;
}

bool SyncCycleSnapshot::has_remaining_local_changes() const {
  return has_remaining_local_changes_;
}

bool SyncCycleSnapshot::is_initialized() const {
  return is_initialized_;
}

const std::vector<int>& SyncCycleSnapshot::num_entries_by_type() const {
  return num_entries_by_type_;
}

const std::vector<int>& SyncCycleSnapshot::num_to_delete_entries_by_type()
    const {
  return num_to_delete_entries_by_type_;
}

sync_pb::SyncEnums::GetUpdatesOrigin SyncCycleSnapshot::get_updates_origin()
    const {
  return get_updates_origin_;
}

base::TimeDelta SyncCycleSnapshot::poll_interval() const {
  return poll_interval_;
}

}  // namespace syncer
