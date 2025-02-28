// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_FINDER_IMPL_H_
#define COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_FINDER_IMPL_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "components/saved_tab_groups/public/collaboration_finder.h"
#include "components/sync/base/collaboration_id.h"

namespace collaboration {

// This class is used to store information about a tab inside a saved tab group.
class CollaborationFinderImpl
    : public tab_groups::CollaborationFinder,
      public data_sharing::DataSharingService::Observer {
 public:
  explicit CollaborationFinderImpl(
      data_sharing::DataSharingService* data_sharing_service);
  ~CollaborationFinderImpl() override;

  // Disallow copy/assign.
  CollaborationFinderImpl(const CollaborationFinderImpl&) = delete;
  CollaborationFinderImpl& operator=(const CollaborationFinderImpl&) = delete;

  // tab_groups::CollaborationFinder overrides.
  void SetClient(Client* client) override;
  bool IsCollaborationAvailable(
      const syncer::CollaborationId& collaboration_id) override;
  void SetCollaborationAvailableForTesting(
      const syncer::CollaborationId& collaboration_id) override;

  // data_sharing::DataSharingService::Observer overrides.
  void OnGroupAdded(const data_sharing::GroupData& group_data,
                    const base::Time& event_time) override;

 private:
  raw_ptr<data_sharing::DataSharingService> data_sharing_service_;
  raw_ptr<Client> client_;
  std::set<syncer::CollaborationId> collaborations_available_for_testing_;
};

}  // namespace collaboration

#endif  // COMPONENTS_COLLABORATION_INTERNAL_COLLABORATION_FINDER_IMPL_H_
