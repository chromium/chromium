// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_UTIL_H_
#define COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_UTIL_H_

#include <stdint.h>

#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/protocol/sync.pb.h"

namespace sync_pb {
class CommitMessage;
}

namespace syncer {

namespace commit_util {

// Adds bookmark extensions activity report to |message|.
void AddExtensionsActivityToMessage(
    ExtensionsActivity* activity,
    ExtensionsActivity::Records* extensions_activity_buffer,
    sync_pb::CommitMessage* message);

// Fills the config_params field of |message|.
void AddClientConfigParamsToMessage(ModelTypeSet enabled_types,
                                    bool cookie_jar_mismatch,
                                    bool single_client,
                                    sync_pb::CommitMessage* message);

}  // namespace commit_util

}  // namespace syncer

#endif  // COMPONENTS_SYNC_ENGINE_IMPL_COMMIT_UTIL_H_
