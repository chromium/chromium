// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SESSIONS_CONTENT_SESSION_TAB_HELPER_DELEGATE_H_
#define COMPONENTS_SESSIONS_CONTENT_SESSION_TAB_HELPER_DELEGATE_H_

#include "components/sessions/core/sessions_export.h"

class SessionID;

namespace sessions {

class SerializedNavigationEntry;
struct SerializedUserAgentOverride;

// Defines the interface used by SessionTabHelper to record changes to
// navigation entries so that they can restored at a later date.
class SESSIONS_EXPORT SessionTabHelperDelegate {
 public:
  // Sets the user agent override of the specified tab.
  virtual void SetTabUserAgentOverride(
      SessionID window_id,
      SessionID tab_id,
      const SerializedUserAgentOverride& user_agent_override) = 0;

  // Sets the index of the selected entry in the navigation controller for the
  // specified tab.
  virtual void SetSelectedNavigationIndex(SessionID window_id,
                                          SessionID tab_id,
                                          int index) = 0;

  // Updates the navigation entry for the specified tab.
  virtual void UpdateTabNavigation(
      SessionID window_id,
      SessionID tab_id,
      const SerializedNavigationEntry& navigation) = 0;

  // Invoked when the NavigationController has removed entries from the list.
  // |index| gives the the starting index from which entries were deleted.
  // |count| gives the number of entries that were removed.
  virtual void TabNavigationPathPruned(SessionID window_id,
                                       SessionID tab_id,
                                       int index,
                                       int count) = 0;

  // Invoked when the NavigationController has deleted entries because of a
  // history deletion.
  virtual void TabNavigationPathEntriesDeleted(SessionID window_id,
                                               SessionID tab_id) = 0;
};

}  // namespace sessions

#endif  // COMPONENTS_SESSIONS_CONTENT_SESSION_TAB_HELPER_DELEGATE_H_
