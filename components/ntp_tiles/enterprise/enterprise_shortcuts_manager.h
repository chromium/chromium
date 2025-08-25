// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_MANAGER_H_
#define COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "components/ntp_tiles/enterprise/enterprise_shortcut.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "url/gurl.h"

namespace ntp_tiles {

// Interface to manage and store enterprise configured custom links for the NTP.
//
// Enterprise custom links are configured by administrators via policy and
// replace the Most Visited tiles. Users can customize the list by reordering,
// editing (name only), and deleting links. Duplicate URLs are not allowed. The
// links and user modifications are stored locally per profile.
//
// The list of links is populated from policy. User customizations are stored
// in prefs and applied on top of the policy-defined links. Admins can control
// whether specific shortcuts may be deleted or modified by the user.
class EnterpriseShortcutsManager {
 public:
  virtual ~EnterpriseShortcutsManager() = default;

  // Clears the user modifications from storage and replaces the current links
  // with shortcuts defined by the enterprise policy.
  virtual void RestorePolicyLinks() = 0;

  // Returns the current links.
  virtual const std::vector<EnterpriseShortcut>& GetLinks() const = 0;

  // Updates the title of the link specified by |url|. Returns false if
  // `allow_user_edit` set to false, |url| is invalid or does not exist in the
  // list, or |title| is empty.
  virtual bool UpdateLink(const GURL& url, const std::u16string& title) = 0;

  // Moves the specified link from its current index and inserts it at
  // |new_pos|. Returns false if |url| is invalid, |url| does not exist in the
  // list, or |new_pos| is invalid/already the current index.
  virtual bool ReorderLink(const GURL& url, size_t new_pos) = 0;

  // Deletes the link with the specified |url|. Returns false if
  // `allow_user_delete` set to false, |url| is invalid, or |url| does not exist
  // in the list.
  virtual bool DeleteLink(const GURL& url) = 0;

  // Restores the previous state of the list of links. Used to undo the previous
  // action (update, reorder, delete). Returns false and does nothing if there
  // is no previous state to restore.
  virtual bool UndoAction() = 0;

  // Registers a callback that will be invoked when enterprise shortcuts are
  // updated by sources other than this interface's methods (i.e. when the
  // policy is updated).
  virtual base::CallbackListSubscription RegisterCallbackForOnChanged(
      base::RepeatingClosure callback) = 0;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_ENTERPRISE_ENTERPRISE_SHORTCUTS_MANAGER_H_
