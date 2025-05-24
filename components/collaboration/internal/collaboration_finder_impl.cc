// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/collaboration/internal/collaboration_finder_impl.h"

#include "base/containers/contains.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/data_sharing/public/group_data.h"
#include "components/sync/base/collaboration_id.h"

using data_sharing::GroupData;
using data_sharing::GroupId;

namespace collaboration {

CollaborationFinderImpl::CollaborationFinderImpl(
    data_sharing::DataSharingService* data_sharing_service)
    : data_sharing_service_(data_sharing_service) {
  data_sharing_service_->AddObserver(this);
}

CollaborationFinderImpl::~CollaborationFinderImpl() {
  data_sharing_service_->RemoveObserver(this);
}

void CollaborationFinderImpl::SetClient(Client* client) {
  CHECK(client);
  CHECK(!client_);
  client_ = client;
}

bool CollaborationFinderImpl::IsCollaborationAvailable(
    const syncer::CollaborationId& collaboration_id) {
  if (base::Contains(collaborations_available_for_testing_, collaboration_id)) {
    return true;
  }

  GroupId group_id(collaboration_id.value());
  return data_sharing_service_->ReadGroup(group_id).has_value();
}

void CollaborationFinderImpl::OnGroupAdded(const GroupData& group_data,
                                           const base::Time& event_time) {
  CHECK(client_);
  GroupId group_id = group_data.group_token.group_id;
  client_->OnCollaborationAvailable(syncer::CollaborationId(*group_id));
}

void CollaborationFinderImpl::SetCollaborationAvailableForTesting(
    const syncer::CollaborationId& collaboration_id) {
  if (collaborations_available_for_testing_.insert(collaboration_id).second) {
    client_->OnCollaborationAvailable(collaboration_id);
  }
}

}  // namespace collaboration
