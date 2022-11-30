// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CUSTOM_LINKS_MANAGER_H_
#define COMPONENTS_NTP_TILES_CUSTOM_LINKS_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "components/ntp_tiles/ntp_tile.h"
#include "url/gurl.h"

namespace ntp_tiles {

// Interface to manage and store custom links for the NTP. Initialized from
// MostVisitedSites.
//
// Custom links replaces the Most Visited tiles and allows users to manually
// add, edit, and delete tiles (i.e. links) up to a certain maximum. Duplicate
// URLs are not allowed, and the links are stored locally per profile.
//
// If the link is initialized from |Initialize|, it is considered a Most Visited
// link and will be deleted when its history entry is cleared. Once the user
// modifies the link, it will no longer be considered Most Visited and will not
// be deleted when history is cleared.
//
// The current list of links is kept in sync with any changes from Chrome sync.
class CustomLinksManager {
 public:
  struct Link {
    GURL url;
    std::u16string title;
    bool is_most_visited = false;

    bool operator==(const Link& other) const {
      return url == other.url && title == other.title &&
             is_most_visited == other.is_most_visited;
    }
  };

  virtual ~CustomLinksManager() = default;

  // Fills the initial links with |tiles| and sets the initalized status to
  // true. These links will be considered Most Visited and will be deleted when
  // history is cleared. Returns false and does nothing if custom links has
  // already been initialized.
  virtual bool Initialize(const NTPTilesVector& tiles) = 0;
  // Uninitializes custom links and clears the current links from storage.
  virtual void Uninitialize() = 0;
  // True if custom links is initialized and Most Visited tiles have been
  // replaced by custom links.
  virtual bool IsInitialized() const = 0;

  // Returns the current links.
  virtual const std::vector<Link>& GetLinks() const = 0;

  // Adds a link to the end of the list. This link will not be deleted when
  // history is cleared. Returns false and does nothing if custom links is not
  // initialized, |url| is invalid, we're at the maximum number of links, or
  // |url| already exists in the list.
  virtual bool AddLink(const GURL& url, const std::u16string& title) = 0;
  // Updates the URL and/or title of the link specified by |url|. The link will
  // no longer be considered Most Visited. Returns false and does nothing if
  // custom links is not initialized, either URL is invalid, |url| does not
  // exist in the list, |new_url| already exists in the list, or both parameters
  // are empty.
  virtual bool UpdateLink(const GURL& url,
                          const GURL& new_url,
                          const std::u16string& new_title) = 0;
  // Moves the specified link from its current index and inserts it at
  // |new_pos|. Returns false and does nothing if custom links is not
  // initialized, |url| is invalid, |url| does not exist in the list, or
  // |new_pos| is invalid/already the current index.
  virtual bool ReorderLink(const GURL& url, size_t new_pos) = 0;
  // Deletes the link with the specified |url|. Returns false and does nothing
  // if custom links is not initialized, |url| is invalid, or |url| does not
  // exist in the list.
  virtual bool DeleteLink(const GURL& url) = 0;
  // Restores the previous state of the list of links. Used to undo the previous
  // action (add, edit, delete, etc.). Returns false and does nothing if custom
  // links is not initialized or there is no previous state to restore.
  virtual bool UndoAction() = 0;

  // Registers a callback that will be invoked when custom links are updated by
  // sources other than this interface's methods (i.e. when links are deleted by
  // history clear or when links are updated by Chrome sync).
  virtual base::CallbackListSubscription RegisterCallbackForOnChanged(
      base::RepeatingClosure callback) = 0;
};

}  // namespace ntp_tiles

#endif  // COMPONENTS_NTP_TILES_CUSTOM_LINKS_MANAGER_H_
