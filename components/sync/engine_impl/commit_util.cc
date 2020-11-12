// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine_impl/commit_util.h"

namespace syncer {

namespace commit_util {

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
  for (auto it = records.begin(); it != records.end(); ++it) {
    sync_pb::ChromiumExtensionsActivity* activity_message =
        message->add_extensions_activity();
    activity_message->set_extension_id(it->second.extension_id);
    activity_message->set_bookmark_writes_since_last_commit(
        it->second.bookmark_write_count);
  }
}

void AddClientConfigParamsToMessage(ModelTypeSet enabled_types,
                                    bool cookie_jar_mismatch,
                                    bool single_client,
                                    sync_pb::CommitMessage* message) {
  sync_pb::ClientConfigParams* config_params = message->mutable_config_params();
  for (ModelType type : enabled_types) {
    if (ProxyTypes().Has(type))
      continue;
    int field_number = GetSpecificsFieldNumberFromModelType(type);
    config_params->mutable_enabled_type_ids()->Add(field_number);
  }
  config_params->set_tabs_datatype_enabled(enabled_types.Has(PROXY_TABS));
  config_params->set_cookie_jar_mismatch(cookie_jar_mismatch);
  config_params->set_single_client(single_client);
}

}  // namespace commit_util

}  // namespace syncer
