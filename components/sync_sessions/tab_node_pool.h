// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_
#define COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_

#include <stddef.h>

#include <map>
#include <set>
#include <string>

#include "base/feature_list.h"
#include "base/macros.h"
#include "components/sessions/core/session_id.h"

namespace sync_sessions {

// A pool for managing free/used tab sync nodes for the *local* session.
// Performs lazy creation of sync nodes when necessary.
// Note: We make use of the following "id's"
// - a tab_id: created by session service, unique to this client
// - a tab_node_id: the id for a particular sync tab node. This is used
//   to generate the sync tab node tag through:
//       tab_tag = StringPrintf("%s %d", local_session_tag, tab_node_id);
//
// A sync node can be in one of the two states:
// 1. Associated   : Sync node is used and associated with a tab.
// 2. Free         : Sync node is unused.

// TODO(crbug.com/882489): Remove feature toggle during code cleanup when a
// satisfying solution is found for closed tabs.
extern const base::Feature kTabNodePoolImmediateDeletion;

class TabNodePool {
 public:
  TabNodePool();
  ~TabNodePool();

  // If free nodes > kFreeNodesHighWatermark, delete all free nodes until
  // free nodes <= kFreeNodesLowWatermark.
  static const size_t kFreeNodesLowWatermark;

  // Maximum limit of FreeNodes allowed on the client.
  static const size_t kFreeNodesHighWatermark;

  static const int kInvalidTabNodeID;

  // Returns the tab node associated with |tab_id| or kInvalidTabNodeID if
  // no association existed.
  int GetTabNodeIdFromTabId(SessionID tab_id) const;

  // Returns the tab_id for |tab_node_id| if it is associated else returns an
  // invalid ID.
  SessionID GetTabIdFromTabNodeId(int tab_node_id) const;

  // Gets the next free tab node (or creates a new one if needed) and associates
  // it to |tab_id|. Returns the tab node ID associated to |tab_id|. |tab_id|
  // must not be previously associated.
  int AssociateWithFreeTabNode(SessionID tab_id);

  // Reassociates |tab_node_id| with |tab_id|. If |tab_node_id| is not already
  // known, it is added to the tab node pool before being associated.
  void ReassociateTabNode(int tab_node_id, SessionID tab_id);

  // Removes association for |tab_id| and returns its tab node to the free node
  // pool.
  void FreeTab(SessionID tab_id);

  // Deletes all free tab nodes. Returns the IDs of the deleted nodes.
  std::set<int> CleanupFreeTabNodes();

  // Deletes all known mappings for |tab_node_id|. As opposed to FreeTab(), it
  // does NOT free the node for later reuse. This is used for foreign sessions
  // when remote deletions are received.
  void DeleteTabNode(int tab_node_id);

  // Returns tab node IDs for all known (used or free) tab nodes.
  std::set<int> GetAllTabNodeIds() const;

  int GetMaxUsedTabNodeIdForTest() const;

 private:
  using TabNodeIDToTabIDMap = std::map<int, SessionID>;
  using TabIDToTabNodeIDMap = std::map<SessionID, int>;

  // Adds |tab_node_id| to the tab node pool.
  // Note: this should only be called when we discover tab sync nodes from
  // previous sessions, not for freeing tab nodes we created through
  // GetTabNodeForTab (use FreeTab for that).
  void AddTabNode(int tab_node_id);

  // Associates |tab_node_id| with |tab_id|. |tab_node_id| must be free. In
  // order to associated a non-free tab node, ReassociateTabNode must be
  // used.
  void AssociateTabNode(int tab_node_id, SessionID tab_id);

  // Stores mapping of node ids associated with tab_ids, these are the used
  // nodes of tab node pool.
  // The nodes in the map can be returned to free tab node pool by calling
  // FreeTab(..).
  TabNodeIDToTabIDMap nodeid_tabid_map_;
  TabIDToTabNodeIDMap tabid_nodeid_map_;

  // The node ids for the set of free sync nodes.
  std::set<int> free_nodes_pool_;

  // The maximum used tab_node id for a sync node.
  int max_used_tab_node_id_;

  // Not actual tab nodes, but instead represent "holes", i.e. tab node IDs
  // that are not used within the range [0..max_used_tab_node_id_). This
  // allows AssociateWithFreeTabNode() to return a compact distribution of IDs.
  std::set<int> missing_nodes_pool_;

  DISALLOW_COPY_AND_ASSIGN(TabNodePool);
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_TAB_NODE_POOL_H_
