// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_COMMIT_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_COMMIT_UTIL_H_

#include <string>
#include <vector>

#include "components/sync/base/data_type.h"
#include "components/sync/base/extensions_activity.h"

namespace sync_pb {
class CommitMessage;
}

namespace syncer::commit_util {

// Adds bookmark extensions activity report to |message|.
void AddExtensionsActivityToMessage(
    ExtensionsActivity* activity,
    ExtensionsActivity::Records* extensions_activity_buffer,
    sync_pb::CommitMessage* message);

// Fills the config_params field of |message|.
void AddClientConfigParamsToMessage(
    DataTypeSet enabled_types,
    bool cookie_jar_mismatch,
    bool single_client,
    bool single_client_with_standalone_invalidations,
    bool single_client_with_old_invalidations,
    const std::vector<std::string>& all_fcm_registration_tokens,
    const std::vector<std::string>&
        fcm_registration_tokens_for_interested_clients,
    sync_pb::CommitMessage* message);

}  // namespace syncer::commit_util

#endif  // COMPONENTS_SYNC_ENGINE_COMMIT_UTIL_H_
