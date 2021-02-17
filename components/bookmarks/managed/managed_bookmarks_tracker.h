// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_TRACKER_H_
#define COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_TRACKER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/prefs/pref_change_registrar.h"

class GURL;
class PrefService;

namespace base {
class ListValue;
}

namespace bookmarks {

class BookmarkModel;
class BookmarkNode;
class BookmarkPermanentNode;

// Tracks either the Managed Bookmarks pref (set by policy) and makes the
// managed_node() in the BookmarkModel follow the policy-defined bookmark tree.
class ManagedBookmarksTracker {
 public:
  using GetManagementDomainCallback = base::RepeatingCallback<std::string()>;

  // Shared constants used in the policy configuration.
  static const char kName[];
  static const char kUrl[];
  static const char kChildren[];
  static const char kFolderName[];

  ManagedBookmarksTracker(BookmarkModel* model,
                          PrefService* prefs,
                          GetManagementDomainCallback callback);
  ~ManagedBookmarksTracker();

  // Returns the initial list of managed bookmarks, which can be passed to
  // LoadInitial() to do the initial load.
  std::unique_ptr<base::ListValue> GetInitialManagedBookmarks();

  // Loads the initial managed bookmarks in |list| into |folder|.
  // New nodes will be assigned IDs starting at |next_node_id|.
  // Returns the next node ID to use.
  static int64_t LoadInitial(BookmarkNode* folder,
                             const base::ListValue* list,
                             int64_t next_node_id);

  // Starts tracking the pref for updates to the managed bookmarks.
  // Should be called after loading the initial bookmarks.
  void Init(BookmarkPermanentNode* managed_node);

 private:
  base::string16 GetBookmarksFolderTitle() const;

  void ReloadManagedBookmarks();

  void UpdateBookmarks(const BookmarkNode* folder, const base::ListValue* list);
  static bool LoadBookmark(const base::ListValue* list,
                           size_t index,
                           base::string16* title,
                           GURL* url,
                           const base::ListValue** children);

  BookmarkModel* model_;
  BookmarkPermanentNode* managed_node_;
  PrefService* prefs_;
  PrefChangeRegistrar registrar_;
  GetManagementDomainCallback get_management_domain_callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedBookmarksTracker);
};

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_MANAGED_MANAGED_BOOKMARKS_TRACKER_H_

