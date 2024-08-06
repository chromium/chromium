// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/commit_util.h"

#include "components/sync/protocol/sync.pb.h"

namespace syncer::commit_util {

void AddExtensionsActivityToMessage(
    ExtensionsActivity* activity,
    ExtensionsActivity::Records* extensions_activity_buffer,
    sync_pb::CommitMessage* message) {
  // This isn't perfect, since the set of extensions activity may not correlate
  // exactly with the items being committed.  That's OK as long as we're looking
  // for a rough estimate of extensions activity, not an precise mapping of
  // which commits were triggered by which extension.
  //
  // We will push this list of extensions activity back into the
  // ExtensionsActivityMonitor if this commit fails.  That's why we must keep a
  // copy of these records in the cycle.
  activity->GetAndClearRecords(extensions_activity_buffer);

  const ExtensionsActivity::Records& records = *extensions_activity_buffer;
  for (const auto& [id, record] : records) {
    sync_pb::ChromiumExtensionsActivity* activity_message =
        message->add_extensions_activity();
    activity_message->set_extension_id(record.extension_id);
    activity_message->set_bookmark_writes_since_last_commit(
        record.bookmark_write_count);
  }
}

void AddClientConfigParamsToMessage(
    DataTypeSet enabled_types,
    bool cookie_jar_mismatch,
    bool single_client,
    bool single_client_with_standalone_invalidations,
    bool single_client_with_old_invalidations,
    const std::vector<std::string>& all_fcm_registration_tokens,
    const std::vector<std::string>&
        fcm_registration_tokens_for_interested_clients,
    sync_pb::CommitMessage* message) {
  sync_pb::ClientConfigParams* config_params = message->mutable_config_params();
  DCHECK(Difference(enabled_types, ProtocolTypes()).empty());
  for (DataType type : enabled_types) {
    int field_number = GetSpecificsFieldNumberFromDataType(type);
    config_params->mutable_enabled_type_ids()->Add(field_number);
  }
  config_params->set_cookie_jar_mismatch(cookie_jar_mismatch);
  config_params->set_single_client(single_client);
  config_params->set_single_client_with_standalone_invalidations(
      single_client_with_standalone_invalidations);
  config_params->set_single_client_with_old_invalidations(
      single_client_with_old_invalidations);
  for (const std::string& token : all_fcm_registration_tokens) {
    *config_params->add_devices_fcm_registration_tokens() = token;
  }
  for (const std::string& token :
       fcm_registration_tokens_for_interested_clients) {
    config_params->add_fcm_registration_tokens_for_interested_clients(token);
  }
}

}  // namespace syncer::commit_util
