// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_COLLABORATION_FINDER_H_
#define COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_COLLABORATION_FINDER_H_

#include "components/sync/base/collaboration_id.h"

namespace tab_groups {

// Interface to provide information about whether a collaboration is available
// or not in DataSharingService. It is as an observer of DataSharingService and
// notifies the client about collaborations (people group) being available
// or removed. This was designed for TabGroupSyncService to be used as the
// client so that there is no direct dependency between the TabGroupSyncService
// and DataSharingService. The client (TabGroupSyncService) will wait for
// collaboration (people group) info to be available on the chrome client before
// notifying the UI when a new shared tab group is received.
class CollaborationFinder {
 public:
  // The client interface that is interested in getting notified about the
  // availability status of collaborations. Meant to be implemented by
  // TabGroupSyncService.
  class Client {
   public:
    // Called to notify that the collaboration corresponding to
    // `collaboration_id` has become available. If the client was waiting on
    // this collaboration, they should proceed.
    virtual void OnCollaborationAvailable(
        const syncer::CollaborationId& collaboration_id) = 0;

   protected:
    virtual ~Client() = default;
  };

  // Method called to set the client.
  virtual void SetClient(Client* client) = 0;

  // Method to query whether a collaboration is currently available.
  virtual bool IsCollaborationAvailable(
      const syncer::CollaborationId& collaboration_id) = 0;

  // For testing only. Marks a collaboration as available.
  virtual void SetCollaborationAvailableForTesting(
      const syncer::CollaborationId& collaboration_id) {}

  virtual ~CollaborationFinder() = default;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_PUBLIC_COLLABORATION_FINDER_H_
