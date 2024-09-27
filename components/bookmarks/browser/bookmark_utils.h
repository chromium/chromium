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
#include "base/strings/utf_offset_string_conversions.h"
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

// Clones bookmark node, adding newly created nodes to |parent| starting at
// |index_to_add_at|. If |reset_node_times| is true cloned bookmarks and
// folders will receive new creation times and folder modification times
// instead of using the values stored in |elements|.
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

// Pastes from the clipboard. The new nodes are added to |parent|, unless
// |parent| is null in which case this does nothing. The nodes are inserted
// at |index|.
void PasteFromClipboard(BookmarkModel* model,
                        const BookmarkNode* parent,
                        size_t index);

// Returns true if the user can copy from the pasteboard.
bool CanPasteFromClipboard(BookmarkModel* model, const BookmarkNode* node);

// Returns a vector containing up to |max_count| of the most recently modified
// user folders. This never returns an empty vector.
std::vector<const BookmarkNode*> GetMostRecentlyModifiedUserFolders(
    BookmarkModel* model, size_t max_count);

// Returns the most recently added bookmarks. This does not return folders,
// only nodes of type url.
void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes);

// Returns true if |n1| was added more recently than |n2|.
bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2);

// Returns the most recently used bookmarks. This does not return folders,
// only nodes of type url. Note: If the bookmarks have the same used time, this
// will return the more recent added bookmarks. Normally, this happens when the
// bookmarks are never used.
void GetMostRecentlyUsedEntries(BookmarkModel* model,
                                size_t count,
                                std::vector<const BookmarkNode*>* nodes);

// Returns up to |max_count| bookmarks from |model| whose url or title contain
// the text |query.word_phrase_query| and exactly match |query.url| and
// |query.title|, for all of the preceding fields that are not NULL.
std::vector<const BookmarkNode*> GetBookmarksMatchingProperties(
    BookmarkModel* model,
    const QueryFields& query,
    size_t max_count);

// Parses the provided query and returns a vector of query words.
std::vector<std::u16string> ParseBookmarkQuery(
    const bookmarks::QueryFields& query);

// Returns true iff |title| or |url| contains each string in |words|. This is
// used when searching for bookmarks.
bool DoesBookmarkContainWords(const std::u16string& title,
                              const GURL& url,
                              const std::vector<std::u16string>& words);

// Register user preferences for Bookmarks Bar.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

// Register managed bookmarks preferences.
void RegisterManagedBookmarksPrefs(PrefRegistrySimple* registry);

// Returns the parent for newly created folders/bookmarks. If |selection| has
// one element and it is a folder, |selection[0]| is returned, otherwise
// |parent| is returned. If |index| is non-null it is set to the index newly
// added nodes should be added at.
const BookmarkNode* GetParentForNewNodes(
    const BookmarkNode* parent,
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>&
        selection,
    size_t* index);

// Deletes the bookmark folders for the given list of |ids|.
void DeleteBookmarkFolders(BookmarkModel* model,
                           const std::vector<int64_t>& ids,
                           const base::Location& location);

// If there are no user bookmarks for url, a bookmark is created.
const BookmarkNode* AddIfNotBookmarked(BookmarkModel* model,
                                       const GURL& url,
                                       const std::u16string& title,
                                       const BookmarkNode* parent = nullptr);

// Removes all bookmarks for the given |url|.
void RemoveAllBookmarks(BookmarkModel* model,
                        const GURL& url,
                        const base::Location& location);

// Truncates an overly-long URL, unescapes it and interprets the
// characters as UTF-8 (both via url_formatter::FormatUrl()), and
// lower-cases it, returning the result. |adjustments|, if non-NULL, is
// set to reflect the transformations the URL spec underwent to become the
// return value.  If a caller computes offsets (e.g., for the position
// of matched text) in this cleaned-up string, it can use |adjustments|
// to calculate the location of these offsets in the original string
// (via base::OffsetAdjuster::UnadjustOffsets()).  This is useful if later
// the original string gets formatted in a different way for displaying.
// In this case, knowing the offsets in the original string will allow them
// to be properly translated to offsets in the newly-formatted string.
//
// The unescaping done by this function makes it possible to match substrings
// that were originally escaped for navigation; for example, if the user
// searched for "a&p", the query would be escaped as "a%26p", so without
// unescaping, an input string of "a&p" would no longer match this URL.  Note
// that the resulting unescaped URL may not be directly navigable (which is
// why it was escaped to begin with).
//
// |url| must be a valid URL.
std::u16string CleanUpUrlForMatching(
    const GURL& gurl,
    base::OffsetAdjuster::Adjustments* adjustments);

// Returns the lower-cased title, possibly truncated if the original title
// is overly-long.
std::u16string CleanUpTitleForMatching(const std::u16string& title);

// Returns true if |url| has a bookmark in the |model| that can be edited
// by the user.
bool IsBookmarkedByUser(BookmarkModel* model, const GURL& url);

// Returns the node with |id|, or NULL if there is no node with |id|.
const BookmarkNode* GetBookmarkNodeByID(const BookmarkModel* model, int64_t id);

// Returns true if |node| is a descendant of |root|.
bool IsDescendantOf(const BookmarkNode* node, const BookmarkNode* root);

// Returns true if any node in |list| is a descendant of |root|.
bool HasDescendantsOf(
    const std::vector<raw_ptr<const BookmarkNode, VectorExperimental>>& list,
    const BookmarkNode* root);

// Returns the parent to add new nodes to, never returns null (as long as
// the model is loaded). If |url| is non-empty, features will have the
// opportunity to suggest contextually relevant folders.
const BookmarkNode* GetParentForNewNodes(BookmarkModel* model,
                                         const GURL& url = GURL());

}  // namespace bookmarks

#endif  // COMPONENTS_BOOKMARKS_BROWSER_BOOKMARK_UTILS_H_
