// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/bookmarks/browser/bookmark_utils.h"

#include <stdint.h>
#include <memory>
#include <unordered_set>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/macros.h"
#include "base/metrics/user_metrics_action.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/query_parser/query_parser.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

using base::Time;

namespace bookmarks {

namespace {

// The maximum length of URL or title returned by the Cleanup functions.
const size_t kCleanedUpUrlMaxLength = 1024u;
const size_t kCleanedUpTitleMaxLength = 1024u;

void CloneBookmarkNodeImpl(BookmarkModel* model,
                           const BookmarkNodeData::Element& element,
                           const BookmarkNode* parent,
                           size_t index_to_add_at,
                           bool reset_node_times) {
  // Make sure to not copy non clonable keys.
  BookmarkNode::MetaInfoMap meta_info_map = element.meta_info_map;
  for (const std::string& key : model->non_cloned_keys())
    meta_info_map.erase(key);

  if (element.is_url) {
    Time date_added = reset_node_times ? Time::Now() : element.date_added;
    DCHECK(!date_added.is_null());

    model->AddURL(parent, index_to_add_at, element.title, element.url,
                  &meta_info_map, date_added);
  } else {
    const BookmarkNode* cloned_node = model->AddFolder(
        parent, index_to_add_at, element.title, &meta_info_map);
    if (!reset_node_times) {
      DCHECK(!element.date_folder_modified.is_null());
      model->SetDateFolderModified(cloned_node, element.date_folder_modified);
    }
    for (int i = 0; i < static_cast<int>(element.children.size()); ++i)
      CloneBookmarkNodeImpl(model, element.children[i], cloned_node, i,
                            reset_node_times);
  }
}

// Comparison function that compares based on date modified of the two nodes.
bool MoreRecentlyModified(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_folder_modified() > n2->date_folder_modified();
}

// Returns true if |text| contains each string in |words|. This is used when
// searching for bookmarks.
bool DoesBookmarkTextContainWords(const base::string16& text,
                                  const std::vector<base::string16>& words) {
  for (size_t i = 0; i < words.size(); ++i) {
    if (!base::i18n::StringSearchIgnoringCaseAndAccents(words[i], text, nullptr,
                                                        nullptr)) {
      return false;
    }
  }
  return true;
}


// This is used with a tree iterator to skip subtrees which are not visible.
bool PruneInvisibleFolders(const BookmarkNode* node) {
  return !node->IsVisible();
}

// This traces parents up to root, determines if node is contained in a
// selected folder.
bool HasSelectedAncestor(BookmarkModel* model,
                         const std::vector<const BookmarkNode*>& selected_nodes,
                         const BookmarkNode* node) {
  if (!node || model->is_permanent_node(node))
    return false;

  for (size_t i = 0; i < selected_nodes.size(); ++i)
    if (node->id() == selected_nodes[i]->id())
      return true;

  return HasSelectedAncestor(model, selected_nodes, node->parent());
}

const BookmarkNode* GetNodeByID(const BookmarkNode* node, int64_t id) {
  if (node->id() == id)
    return node;

  for (const auto& child : node->children()) {
    const BookmarkNode* result = GetNodeByID(child.get(), id);
    if (result)
      return result;
  }
  return nullptr;
}

// Attempts to shorten a URL safely (i.e., by preventing the end of the URL
// from being in the middle of an escape sequence) to no more than
// kCleanedUpUrlMaxLength characters, returning the result.
std::string TruncateUrl(const std::string& url) {
  if (url.length() <= kCleanedUpUrlMaxLength)
    return url;

  // If we're in the middle of an escape sequence, truncate just before it.
  if (url[kCleanedUpUrlMaxLength - 1] == '%')
    return url.substr(0, kCleanedUpUrlMaxLength - 1);
  if (url[kCleanedUpUrlMaxLength - 2] == '%')
    return url.substr(0, kCleanedUpUrlMaxLength - 2);

  return url.substr(0, kCleanedUpUrlMaxLength);
}

// Returns the URL from the clipboard. If there is no URL an empty URL is
// returned.
GURL GetUrlFromClipboard() {
  base::string16 url_text;
#if !defined(OS_IOS)
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, &url_text);
#endif
  return GURL(url_text);
}

class VectorIterator {
 public:
  explicit VectorIterator(std::vector<const BookmarkNode*>* nodes)
      : nodes_(nodes), current_(nodes->begin()) {}
  bool has_next() { return (current_ != nodes_->end()); }
  const BookmarkNode* Next() {
    const BookmarkNode* result = *current_;
    ++current_;
    return result;
  }

 private:
  std::vector<const BookmarkNode*>* nodes_;
  std::vector<const BookmarkNode*>::iterator current_;

  DISALLOW_COPY_AND_ASSIGN(VectorIterator);
};

template <class type>
void GetBookmarksMatchingPropertiesImpl(
    type& iterator,
    BookmarkModel* model,
    const QueryFields& query,
    const std::vector<base::string16>& query_words,
    size_t max_count,
    std::vector<const BookmarkNode*>* nodes) {
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if ((!query_words.empty() &&
         !DoesBookmarkContainWords(node->GetTitle(), node->url(),
                                   query_words)) ||
        model->is_permanent_node(node)) {
      continue;
    }
    if (query.title && node->GetTitle() != *query.title)
      continue;

    nodes->push_back(node);
    if (nodes->size() == max_count)
      return;
  }
}

#if defined(OS_ANDROID)
// Returns whether or not a bookmark model contains any bookmarks aside of the
// permanent nodes.
bool HasUserCreatedBookmarks(BookmarkModel* model) {
  const BookmarkNode* root_node = model->root_node();

  return std::any_of(
      root_node->children().cbegin(), root_node->children().cend(),
      [](const auto& node) { return !node->children().empty(); });
}
#endif

}  // namespace

QueryFields::QueryFields() {}
QueryFields::~QueryFields() {}

void CloneBookmarkNode(BookmarkModel* model,
                       const std::vector<BookmarkNodeData::Element>& elements,
                       const BookmarkNode* parent,
                       size_t index_to_add_at,
                       bool reset_node_times) {
  if (!parent->is_folder() || !model) {
    NOTREACHED();
    return;
  }
  for (size_t i = 0; i < elements.size(); ++i) {
    CloneBookmarkNodeImpl(model, elements[i], parent, index_to_add_at + i,
                          reset_node_times);
  }
}

void CopyToClipboard(BookmarkModel* model,
                     const std::vector<const BookmarkNode*>& nodes,
                     bool remove_nodes) {
  if (nodes.empty())
    return;

  // Create array of selected nodes with descendants filtered out.
  std::vector<const BookmarkNode*> filtered_nodes;
  for (size_t i = 0; i < nodes.size(); ++i)
    if (!HasSelectedAncestor(model, nodes, nodes[i]->parent()))
      filtered_nodes.push_back(nodes[i]);

  BookmarkNodeData(filtered_nodes).WriteToClipboard();

  if (remove_nodes) {
    ScopedGroupBookmarkActions group_cut(model);
    for (size_t i = 0; i < filtered_nodes.size(); ++i) {
      int index = filtered_nodes[i]->parent()->GetIndexOf(filtered_nodes[i]);
      if (index > -1)
        model->Remove(filtered_nodes[i]);
    }
  }
}

// Updates |title| such that |url| and |title| pair are unique among the
// children of |parent|.
void MakeTitleUnique(const BookmarkModel* model,
                     const BookmarkNode* parent,
                     const GURL& url,
                     base::string16* title) {
  std::unordered_set<base::string16> titles;
  base::string16 original_title_lower = base::i18n::ToLower(*title);
  for (const auto& node : parent->children()) {
    if (node->is_url() && (url == node->url()) &&
        base::StartsWith(base::i18n::ToLower(node->GetTitle()),
                         original_title_lower,
                         base::CompareCase::SENSITIVE)) {
      titles.insert(node->GetTitle());
    }
  }

  if (titles.find(*title) == titles.end())
    return;

  for (size_t i = 0; i < titles.size(); i++) {
    const base::string16 new_title(*title +
                                   base::ASCIIToUTF16(base::StringPrintf(
                                       " (%lu)", (unsigned long)(i + 1))));
    if (titles.find(new_title) == titles.end()) {
      *title = new_title;
      return;
    }
  }
  NOTREACHED();
}

void PasteFromClipboard(BookmarkModel* model,
                        const BookmarkNode* parent,
                        size_t index) {
  if (!parent)
    return;

  BookmarkNodeData bookmark_data;
  if (!bookmark_data.ReadFromClipboard(ui::ClipboardBuffer::kCopyPaste)) {
    GURL url = GetUrlFromClipboard();
    if (!url.is_valid())
      return;
    BookmarkNode node(/*id=*/0, base::GenerateGUID(), url);
    node.SetTitle(base::ASCIIToUTF16(url.spec()));
    bookmark_data = BookmarkNodeData(&node);
  }
  DCHECK_LE(index, parent->children().size());
  ScopedGroupBookmarkActions group_paste(model);

  if (bookmark_data.size() == 1 &&
      model->IsBookmarked(bookmark_data.elements[0].url)) {
    MakeTitleUnique(model,
                    parent,
                    bookmark_data.elements[0].url,
                    &bookmark_data.elements[0].title);
  }

  CloneBookmarkNode(model, bookmark_data.elements, parent, index, true);
}

bool CanPasteFromClipboard(BookmarkModel* model, const BookmarkNode* node) {
  if (!node || !model->client()->CanBeEditedByUser(node))
    return false;
  return (BookmarkNodeData::ClipboardContainsBookmarks() ||
          GetUrlFromClipboard().is_valid());
}

std::vector<const BookmarkNode*> GetMostRecentlyModifiedUserFolders(
    BookmarkModel* model,
    size_t max_count) {
  std::vector<const BookmarkNode*> nodes;
  ui::TreeNodeIterator<const BookmarkNode> iterator(
      model->root_node(), base::Bind(&PruneInvisibleFolders));

  while (iterator.has_next()) {
    const BookmarkNode* parent = iterator.Next();
    if (!model->client()->CanBeEditedByUser(parent))
      continue;
    if (parent->is_folder() && parent->date_folder_modified() > Time()) {
      if (max_count == 0) {
        nodes.push_back(parent);
      } else {
        auto i = std::upper_bound(nodes.begin(), nodes.end(), parent,
                                  &MoreRecentlyModified);
        if (nodes.size() < max_count || i != nodes.end()) {
          nodes.insert(i, parent);
          while (nodes.size() > max_count)
            nodes.pop_back();
        }
      }
    }  // else case, the root node, which we don't care about or imported nodes
       // (which have a time of 0).
  }

  if (nodes.size() < max_count) {
    // Add the permanent nodes if there is space. The permanent nodes are the
    // only children of the root_node.
    const BookmarkNode* root_node = model->root_node();

    for (const auto& node : root_node->children()) {
      if (node->IsVisible() && model->client()->CanBeEditedByUser(node.get()) &&
          !base::Contains(nodes, node.get())) {
        nodes.push_back(node.get());

        if (nodes.size() == max_count)
          break;
      }
    }
  }
  return nodes;
}

void GetMostRecentlyAddedEntries(BookmarkModel* model,
                                 size_t count,
                                 std::vector<const BookmarkNode*>* nodes) {
  ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
  while (iterator.has_next()) {
    const BookmarkNode* node = iterator.Next();
    if (node->is_url()) {
      auto insert_position = std::upper_bound(nodes->begin(), nodes->end(),
                                              node, &MoreRecentlyAdded);
      if (nodes->size() < count || insert_position != nodes->end()) {
        nodes->insert(insert_position, node);
        while (nodes->size() > count)
          nodes->pop_back();
      }
    }
  }
}

bool MoreRecentlyAdded(const BookmarkNode* n1, const BookmarkNode* n2) {
  return n1->date_added() > n2->date_added();
}

void GetBookmarksMatchingProperties(BookmarkModel* model,
                                    const QueryFields& query,
                                    size_t max_count,
                                    std::vector<const BookmarkNode*>* nodes) {
  std::vector<base::string16> query_words = ParseBookmarkQuery(query);
  if (query.word_phrase_query && query_words.empty())
    return;

  if (query.url) {
    // Shortcut into the BookmarkModel if searching for URL.
    GURL url(*query.url);
    std::vector<const BookmarkNode*> url_matched_nodes;
    if (url.is_valid())
      model->GetNodesByURL(url, &url_matched_nodes);
    VectorIterator iterator(&url_matched_nodes);
    GetBookmarksMatchingPropertiesImpl<VectorIterator>(
        iterator, model, query, query_words, max_count, nodes);
  } else {
    ui::TreeNodeIterator<const BookmarkNode> iterator(model->root_node());
    GetBookmarksMatchingPropertiesImpl<
        ui::TreeNodeIterator<const BookmarkNode>>(
        iterator, model, query, query_words, max_count, nodes);
  }
}

// Parses the provided query and returns a vector of query words.
std::vector<base::string16> ParseBookmarkQuery(
    const bookmarks::QueryFields& query) {
  std::vector<base::string16> query_words;
  query_parser::QueryParser parser;
  if (query.word_phrase_query) {
    parser.ParseQueryWords(base::i18n::ToLower(*query.word_phrase_query),
                           query_parser::MatchingAlgorithm::DEFAULT,
                           &query_words);
  }
  return query_words;
}

// Returns true if |node|s title or url contains the strings in |words|.
bool DoesBookmarkContainWords(const base::string16& title,
                              const GURL& url,
                              const std::vector<base::string16>& words) {
  return DoesBookmarkTextContainWords(title, words) ||
         DoesBookmarkTextContainWords(base::UTF8ToUTF16(url.spec()), words) ||
         DoesBookmarkTextContainWords(
             url_formatter::FormatUrl(url, url_formatter::kFormatUrlOmitNothing,
                                      net::UnescapeRule::NORMAL, nullptr,
                                      nullptr, nullptr),
             words);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kShowBookmarkBar,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(prefs::kEditBookmarksEnabled, true);
  registry->RegisterBooleanPref(
      prefs::kShowAppsShortcutInBookmarkBar,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      prefs::kShowManagedBookmarksInBookmarkBar,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  RegisterManagedBookmarksPrefs(registry);
}

void RegisterManagedBookmarksPrefs(PrefRegistrySimple* registry) {
  // Don't sync this, as otherwise, due to a limitation in sync, it
  // will cause a deadlock (see http://crbug.com/97955).  If we truly
  // want to sync the expanded state of folders, it should be part of
  // bookmark sync itself (i.e., a property of the sync folder nodes).
  registry->RegisterListPref(prefs::kBookmarkEditorExpandedNodes);
  registry->RegisterListPref(prefs::kManagedBookmarks);
  registry->RegisterStringPref(
      prefs::kManagedBookmarksFolderName, std::string());
}

const BookmarkNode* GetParentForNewNodes(
    const BookmarkNode* parent,
    const std::vector<const BookmarkNode*>& selection,
    size_t* index) {
  const BookmarkNode* real_parent = parent;

  if (selection.size() == 1 && selection[0]->is_folder())
    real_parent = selection[0];

  if (index) {
    if (selection.size() == 1 && selection[0]->is_url()) {
      *index = size_t{real_parent->GetIndexOf(selection[0]) + 1};
      DCHECK_NE(0u, *index);
    } else {
      *index = real_parent->children().size();
    }
  }

  return real_parent;
}

void DeleteBookmarkFolders(BookmarkModel* model,
                           const std::vector<int64_t>& ids) {
  // Remove the folders that were removed. This has to be done after all the
  // other changes have been committed.
  for (auto iter = ids.begin(); iter != ids.end(); ++iter) {
    const BookmarkNode* node = GetBookmarkNodeByID(model, *iter);
    if (!node)
      continue;
    model->Remove(node);
  }
}

void AddIfNotBookmarked(BookmarkModel* model,
                        const GURL& url,
                        const base::string16& title) {
  if (IsBookmarkedByUser(model, url))
    return;  // Nothing to do, a user bookmark with that url already exists.
  model->client()->RecordAction(base::UserMetricsAction("BookmarkAdded"));
  const BookmarkNode* parent = GetParentForNewNodes(model);
  model->AddURL(parent, parent->children().size(), title, url);
}

void RemoveAllBookmarks(BookmarkModel* model, const GURL& url) {
  std::vector<const BookmarkNode*> bookmarks;
  model->GetNodesByURL(url, &bookmarks);

  // Remove all the user bookmarks.
  for (size_t i = 0; i < bookmarks.size(); ++i) {
    const BookmarkNode* node = bookmarks[i];
    int index = node->parent()->GetIndexOf(node);
    if (index > -1 && model->client()->CanBeEditedByUser(node))
      model->Remove(node);
  }
}

base::string16 CleanUpUrlForMatching(
    const GURL& gurl,
    base::OffsetAdjuster::Adjustments* adjustments) {
  base::OffsetAdjuster::Adjustments tmp_adjustments;
  return base::i18n::ToLower(url_formatter::FormatUrlWithAdjustments(
      GURL(TruncateUrl(gurl.spec())),
      url_formatter::kFormatUrlOmitUsernamePassword,
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
          net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
      nullptr, nullptr, adjustments ? adjustments : &tmp_adjustments));
}

base::string16 CleanUpTitleForMatching(const base::string16& title) {
  return base::i18n::ToLower(title.substr(0u, kCleanedUpTitleMaxLength));
}

bool CanAllBeEditedByUser(BookmarkClient* client,
                          const std::vector<const BookmarkNode*>& nodes) {
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (!client->CanBeEditedByUser(nodes[i]))
      return false;
  }
  return true;
}

bool IsBookmarkedByUser(BookmarkModel* model, const GURL& url) {
  std::vector<const BookmarkNode*> nodes;
  model->GetNodesByURL(url, &nodes);
  for (size_t i = 0; i < nodes.size(); ++i) {
    if (model->client()->CanBeEditedByUser(nodes[i]))
      return true;
  }
  return false;
}

const BookmarkNode* GetBookmarkNodeByID(const BookmarkModel* model,
                                        int64_t id) {
  // TODO(sky): TreeNode needs a method that visits all nodes using a predicate.
  return GetNodeByID(model->root_node(), id);
}

bool IsDescendantOf(const BookmarkNode* node, const BookmarkNode* root) {
  return node && node->HasAncestor(root);
}

bool HasDescendantsOf(const std::vector<const BookmarkNode*>& list,
                      const BookmarkNode* root) {
  for (const BookmarkNode* node : list) {
    if (IsDescendantOf(node, root))
      return true;
  }
  return false;
}

const BookmarkNode* GetParentForNewNodes(BookmarkModel* model) {
#if defined(OS_ANDROID)
  if (!HasUserCreatedBookmarks(model))
    return model->mobile_node();
#endif
  std::vector<const BookmarkNode*> nodes =
      GetMostRecentlyModifiedUserFolders(model, 1);
  DCHECK(!nodes.empty());  // This list is always padded with default folders.
  return nodes[0];
}

}  // namespace bookmarks
