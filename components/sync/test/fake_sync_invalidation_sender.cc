// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_invalidation_sender.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/sync/base/time.h"
#include "components/sync/protocol/sync_invalidations_payload.pb.h"

namespace fake_server {

FakeSyncInvalidationSender::FakeSyncInvalidationSender() = default;

FakeSyncInvalidationSender::~FakeSyncInvalidationSender() = default;

void FakeSyncInvalidationSender::InitializeAndListen(
    net::test_server::EmbeddedTestServer& embedded_test_server) {}

void FakeSyncInvalidationSender::AppendCommandLineSwitches(
    base::CommandLine* cl,
    const net::test_server::EmbeddedTestServer& embedded_test_server) const {}

void FakeSyncInvalidationSender::Start(FakeServer* fake_server,
                                       const base::FilePath& storage_dir) {
  CHECK(fake_server);
  CHECK(!storage_dir.empty());
  CHECK(!fake_server_observation_.IsObserving());
  fake_server_observation_.Observe(fake_server);
}

void FakeSyncInvalidationSender::OnWillCommit() {
  token_to_interested_data_types_.clear();
  UpdateTokenToInterestedDataTypesMap();
}

void FakeSyncInvalidationSender::OnCommit(
    syncer::DataTypeSet committed_data_types) {
  // Update token to interested data types mapping. This is needed to support
  // newly added DeviceInfos during commit request.
  UpdateTokenToInterestedDataTypesMap();

  base::flat_map<std::string, sync_pb::SyncInvalidationsPayload>
      token_to_payload;
  for (const auto& [token, interested_data_types] :
       token_to_interested_data_types_) {
    // Send the invalidation only for interested types.
    const syncer::DataTypeSet invalidated_data_types =
        Intersection(committed_data_types, interested_data_types);
    if (invalidated_data_types.empty()) {
      continue;
    }

    sync_pb::SyncInvalidationsPayload payload;
    for (const syncer::DataType data_type : invalidated_data_types) {
      payload.add_data_type_invalidations()->set_data_type_id(
          syncer::GetSpecificsFieldNumberFromDataType(data_type));
    }

    // Versions are used to keep hints ordered. SyncEngineBackend interprets the
    // version as microseconds since the Unix epoch when calculating transit
    // latency. Additionally, set the dedicated publish and dispatch time
    // fields.
    payload.set_version(
        (base::Time::Now() - base::Time::UnixEpoch()).InMicroseconds());
    payload.set_server_publish_time_unix_epoch_millis(
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
    payload.set_server_dispatch_time_unix_epoch_millis(
        (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
    payload.set_hint("hint");

    token_to_payload.emplace(token, std::move(payload));
  }

  SendInvalidations(std::move(token_to_payload));
}

void FakeSyncInvalidationSender::UpdateTokenToInterestedDataTypesMap() {
  CHECK(fake_server_observation_.IsObserving());
  base::flat_map<std::string, base::Time> token_to_mtime;
  for (const sync_pb::SyncEntity& entity :
       fake_server_observation_.GetSource()->GetSyncEntitiesByDataType(
           syncer::DEVICE_INFO)) {
    const sync_pb::InvalidationSpecificFields& invalidation_fields =
        entity.specifics().device_info().invalidation_fields();
    const std::string& token = invalidation_fields.instance_id_token();
    if (token.empty()) {
      continue;
    }

    // If several DeviceInfos have the same FCM registration token, select the
    // latest updated one. This may happen after resetting sync engine and
    // changing cache GUID without signout.
    // TODO(crbug.com/40225423): remove once fixed.
    const base::Time last_updated = syncer::ProtoTimeToTime(
        entity.specifics().device_info().last_updated_timestamp());
    auto it = token_to_mtime.find(token);
    if (it != token_to_mtime.end() && it->second >= last_updated) {
      continue;
    }

    token_to_mtime[token] = last_updated;
    token_to_interested_data_types_[token] = syncer::DataTypeSet();
    for (const int field_number :
         invalidation_fields.interested_data_type_ids()) {
      const syncer::DataType data_type =
          syncer::GetDataTypeFromSpecificsFieldNumber(field_number);
      CHECK(syncer::IsRealDataType(data_type));
      token_to_interested_data_types_[token].Put(data_type);
    }
  }
}

}  // namespace fake_server
