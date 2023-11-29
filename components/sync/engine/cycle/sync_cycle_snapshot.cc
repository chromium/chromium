// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/cycle/sync_cycle_snapshot.h"

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/sync/protocol/proto_enum_conversions.h"

namespace syncer {

namespace {

std::u16string FormatTimeDelta(base::TimeDelta delta) {
  std::u16string value;
  bool ok =
      base::TimeDurationFormat(delta, base::DURATION_WIDTH_NARROW, &value);
  DCHECK(ok);
  return value;
}

}  // namespace

SyncCycleSnapshot::SyncCycleSnapshot()
    : is_silenced_(false),
      num_server_conflicts_(0),
      notifications_enabled_(false),
      has_remaining_local_changes_(false),
      is_initialized_(false) {}

SyncCycleSnapshot::SyncCycleSnapshot(
    const std::string& birthday,
    const std::string& bag_of_chips,
    const ModelNeutralState& model_neutral_state,
    const ProgressMarkerMap& download_progress_markers,
    bool is_silenced,
    int num_server_conflicts,
    bool notifications_enabled,
    base::Time sync_start_time,
    base::Time poll_finish_time,
    sync_pb::SyncEnums::GetUpdatesOrigin get_updates_origin,
    base::TimeDelta poll_interval,
    bool has_remaining_local_changes)
    : birthday_(birthday),
      bag_of_chips_(bag_of_chips),
      model_neutral_state_(model_neutral_state),
      download_progress_markers_(download_progress_markers),
      is_silenced_(is_silenced),
      num_server_conflicts_(num_server_conflicts),
      notifications_enabled_(notifications_enabled),
      sync_start_time_(sync_start_time),
      poll_finish_time_(poll_finish_time),
      get_updates_origin_(get_updates_origin),
      poll_interval_(poll_interval),
      has_remaining_local_changes_(has_remaining_local_changes),
      is_initialized_(true) {}

SyncCycleSnapshot::SyncCycleSnapshot(const SyncCycleSnapshot& other) = default;

SyncCycleSnapshot::~SyncCycleSnapshot() = default;

base::Value::Dict SyncCycleSnapshot::ToValue() const {
  std::string encoded_bag_of_chips = base::Base64Encode(bag_of_chips_);

  return base::Value::Dict()
      .Set("birthday", birthday_)
      .Set("bagOfChips", encoded_bag_of_chips)
      .Set("numSuccessfulCommits", model_neutral_state_.num_successful_commits)
      .Set("numSuccessfulBookmarkCommits",
           model_neutral_state_.num_successful_bookmark_commits)
      .Set("numUpdatesDownloadedTotal",
           model_neutral_state_.num_updates_downloaded_total)
      .Set("numTombstoneUpdatesDownloadedTotal",
           model_neutral_state_.num_tombstone_updates_downloaded_total)
      .Set("downloadProgressMarkers",
           ProgressMarkerMapToValueDict(download_progress_markers_))
      .Set("isSilenced", is_silenced_)
      // We don't care too much if we lose precision here, also.
      .Set("numServerConflicts", num_server_conflicts_)
      .Set("getUpdatesOrigin", ProtoEnumToString(get_updates_origin_))
      .Set("notificationsEnabled", notifications_enabled_)
      .Set("hasRemainingLocalChanges", has_remaining_local_changes_)
      .Set("poll_interval", FormatTimeDelta(poll_interval_))
      .Set("poll_finish_time",
           base::TimeFormatShortDateAndTimeWithTimeZone(poll_finish_time_));
}

std::string SyncCycleSnapshot::ToString() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      ToValue(), base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

const ProgressMarkerMap& SyncCycleSnapshot::download_progress_markers() const {
  return download_progress_markers_;
}

bool SyncCycleSnapshot::is_silenced() const {
  return is_silenced_;
}

int SyncCycleSnapshot::num_server_conflicts() const {
  return num_server_conflicts_;
}

bool SyncCycleSnapshot::notifications_enabled() const {
  return notifications_enabled_;
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

sync_pb::SyncEnums::GetUpdatesOrigin SyncCycleSnapshot::get_updates_origin()
    const {
  return get_updates_origin_;
}

base::TimeDelta SyncCycleSnapshot::poll_interval() const {
  return poll_interval_;
}

}  // namespace syncer
