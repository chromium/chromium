// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UTILS_H_
#define COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UTILS_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/prefs/pref_registry_simple.h"

class GURL;

namespace user_prefs {
class PrefRegistrySyncable;
}

// A collection of bookmark utility functions used by various parts of the UI
// that show bookmarks (bookmark manager, bookmark bar view, ...) and other
// systems that involve indexing and searching bookmarks.
namespace bookmarks {

class BookmarkModel;
class BookmarkNode;

// Fields to use when finding matching bookmarks.
struct QueryFields {
  QueryFields();
  ~QueryFields();

  std::unique_ptr<std::u16string> word_phrase_query;
  std::unique_ptr<std::u16string> url;
  std::unique_ptr<std::u16string> title;
};

class VectorIterator {
 public:
  explicit VectorIterator(
      std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>* nodes);
  VectorIterator(const VectorIterator& other) = delete;
  VectorIterator& operator=(const VectorIterator& other) = delete;
  ~VectorIterator();
  bool has_next();
  const BookmarkNode* Next();

 private:
  raw_ptr<std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>> nodes_;
  std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>::iterator
      current_;
};

// Clones bookmark node, adding newly created nodes to `parent` starting at
// `index_to_add_at`. If `reset_node_times` is true cloned bookmarks and
// folders will receive new creation times and folder modification times
// instead of using the values stored in `elements`.
void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       size_t index_to_add_at,
                       bool reset_node_times);

// Copies nodes onto the clipboard. If `remove_nodes` is true the nodes are
// removed after copied to the clipboard. The nodes are copied in such a way
// that if pasted again copies are made. Pass the calling context through as
// `source`.
void CopyToClipboard(
    BookmarkModel* model,
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& nodes,
    bool remove_nodes,
    metrics::BookmarkEditSource source,
    bool is_off_the_record);

// Returns a vector containing of the most recently modified user folders. This
// never returns an empty vector.
std::vector<const BookmarkNode*> GetMostRecentlyModifiedUserFolders(
    BookmarkModel* model);

// If this should be used on mobile we need to reevaluate if this implementation
// makes sense. See tests in bookmark_utils_unittest.cc which currently fail
// outside of desktop. Enable and update those if this is to be used on mobile.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Bookmark nodes, split by account/local bookmarks.
struct BookmarkNodesSplitByAccountAndLocal final {
  BookmarkNodesSplitByAccountAndLocal();
  BookmarkNodesSplitByAccountAndLocal(
      const BookmarkNodesSplitByAccountAndLocal&);
  BookmarkNodesSplitByAccountAndLocal& operator=(
      const BookmarkNodesSplitByAccountAndLocal&);
  ~BookmarkNodesSplitByAccountAndLocal();

  std::vector<const BookmarkNode*> account_nodes;
  std::vector<const BookmarkNode*> local_nodes;
};

// Get recently-used-folders, including permanent nodes for display split up by
// "account" and "local" nodes. If there are no "account" bookmarks all entries
// are returned as local nodes even if sync'd, to be displayed as a single list
// without any headers/labels.
//
// In case of a mixed account and local bookmarks in the MRU nodes, this would
// display:
//   - Account Bookmark Heading
//     - Most recently used custom account bookmarks
//     - Account permanent folders if visible
//   - Local Bookmark Heading
//     - Most recently used custom local bookmarks
//     - Local permanent folder if it is the most recently used folder
//
// If MRU nodes are only local or account, this would display:
//   - Most recently used custom
//   - Account/Local and syncable permanent nodes
//
// Note: The parent of `display_node` is pushed on top of its corresponding list
// if it is a non-permanent folder or at the end if it is a permanent folder
// that is not already included.
BookmarkNodesSplitByAccountAndLocal GetMostRecentlyUsedFoldersForDisplay(
    BookmarkModel* model,
    const BookmarkNode* displayed_node);

// Returns permanent nodes for display. Either account or local+syncable types
// are always available, sometimes both (non-empty visible local permanent
// nodes).
BookmarkNodesSplitByAccountAndLocal GetPermanentNodesForDisplay(
    const BookmarkModel* model);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Returns true if any local permanent nodes contain bookmarks.
bool HasLocalOrSyncableBookmarks(const BookmarkModel* model);

// Returns the most recently added bookmarks. This does not return folders,
// only nodes of type url.
void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes);

// Returns true if `n1` was added more recently than `n2`.
bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2);

// Returns the most recently used bookmarks. This does not return folders,
// only nodes of type url. Note: If the bookmarks have the same used time, this
// will return the more recent added bookmarks. Normally, this happens when the
// bookmarks are never used.
void GetMostRecentlyUsedEntries(BookmarkModel* model,
                                size_t count,
                                std::vector<const BookmarkNode*>* nodes);

// Returns up to `max_count` bookmarks from `model` whose url or title contain
// the text `query.word_phrase_query` and exactly match `query.url` and
// `query.title`, for all of the preceding fields that are not NULL.
std::vector<const BookmarkNode*> GetBookmarksMatchingProperties(
    BookmarkModel* model,
    const QueryFields& query,
    size_t max_count);

// Parses the provided query and returns a vector of query words.
std::vector<std::u16string> ParseBookmarkQuery(
    const bookmarks::QueryFields& query);

// Returns true iff `title` or `url` contains each string in `words`. This is
// used when searching for bookmarks.
bool DoesBookmarkContainWords(const std::u16string& title,
                              const GURL& url,
                              const std::vector<std::u16string>& words);

// Register user preferences for Bookmarks Bar.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Register managed bookmarks preferences.
void RegisterManagedBookmarksPrefs(PrefRegistrySimple* registry);

// Deletes the bookmark folders for the given list of `ids`.
void DeleteBookmarkFolders(BookmarkModel* model,
                           const std::vector<int64_t>& ids,
                           const base::Location& location);

// If there are no user bookmarks for url, a bookmark is created.
const BookmarkNode* AddIfNotBookmarked(BookmarkModel* model,
                                       const GURL& url,
                                       const std::u16string& title);

// Removes all bookmarks for the given `url`.
void RemoveAllBookmarks(BookmarkModel* model,
                        const GURL& url,
                        const base::Location& location);

// Returns true if `url` has a bookmark in the `model` that can be edited
// by the user.
bool IsBookmarkedByUser(BookmarkModel* model, const GURL& url);

// Returns the node with `id`, or NULL if there is no node with `id`.
const BookmarkNode* GetBookmarkNodeByID(const BookmarkModel* model, int64_t id);

// Returns true if `node` is a descendant of `root`.
bool IsDescendantOf(const BookmarkNode* node, const BookmarkNode* root);

// Returns true if any node in `list` is a descendant of `root`.
bool HasDescendantsOf(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& list,
    const BookmarkNode* root);

// Returns the parent to add new nodes to, never returns null (as long as
// the model is loaded). If `url` is non-empty, features will have the
// opportunity to suggest contextually relevant folders.
const BookmarkNode* GetParentForNewNodes(BookmarkModel* model,
                                         const GURL& url = GURL());

// This pruning keeps visible, non-managed folder nodes.
bool PruneFoldersForDisplay(const BookmarkModel* model,
                            const BookmarkNode* node);

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UTILS_H_
