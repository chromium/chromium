// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_COLLABORATION_GROUP_UTIL_H_
#define COMPONENTS_SYNC_TEST_COLLABORATION_GROUP_UTIL_H_

#include "components/sync/base/time.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/collaboration_group_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/test/entity_builder_factory.h"
#include "components/sync/test/fake_server.h"

// Util class with methods to facilitate writing unit tests for
// injecting/updating entities for collaboration group in fake
// sync server.
namespace collaboration_group_utils {

sync_pb::CollaborationGroupSpecifics MakeCollaborationGroupSpecifics(
    const std::string& id);

syncer::EntityData EntityDataFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics);

std::unique_ptr<syncer::EntityChange> EntityChangeAddFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics);

std::unique_ptr<syncer::EntityChange> EntityChangeUpdateFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics);

std::unique_ptr<syncer::EntityChange> EntityChangeDeleteFromSpecifics(
    const sync_pb::CollaborationGroupSpecifics& specifics);

}  // namespace collaboration_group_utils

#endif  // COMPONENTS_SYNC_TEST_COLLABORATION_GROUP_UTIL_H_
