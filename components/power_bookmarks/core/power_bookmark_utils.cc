// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/core/power_bookmark_utils.h"

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/i18n/string_search.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/power_bookmarks/core/proto/power_bookmark_meta.pb.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

namespace power_bookmarks {

namespace {

// Backfill old shopping_specifics field to the new one. This is necessary
// as we're transitioning from oneof powers to allowing multiple.
// TODO(crbug.com/40233844): Also invoke this in meta updates once available.
void BackfillShoppingSpecifics(PowerBookmarkMeta* meta) {
  if (meta->has_old_shopping_specifics() && !meta->has_shopping_specifics()) {
    meta->mutable_shopping_specifics()->CopyFrom(
        meta->old_shopping_specifics());
    meta->clear_old_shopping_specifics();
  }
}

}  // namespace

const char kPowerBookmarkMetaKey[] = "power_bookmark_meta";

PowerBookmarkQueryFields::PowerBookmarkQueryFields() = default;
PowerBookmarkQueryFields::~PowerBookmarkQueryFields() = default;

std::unique_ptr<PowerBookmarkMeta> GetNodePowerBookmarkMeta(
    bookmarks::BookmarkModel* model,
    const bookmarks::BookmarkNode* node) {
  if (!model)
    return nullptr;

  std::string raw_meta;
  if (!node || !node->GetMetaInfo(kPowerBookmarkMetaKey, &raw_meta))
    return nullptr;

  std::unique_ptr<PowerBookmarkMeta> meta =
      std::make_unique<PowerBookmarkMeta>();
  if (!DecodeMetaFromStorage(raw_meta, meta.get())) {
    meta.reset();
    DeleteNodePowerBookmarkMeta(model, node);
  }

  BackfillShoppingSpecifics(meta.get());

  return meta;
}

void SetNodePowerBookmarkMeta(bookmarks::BookmarkModel* model,
                              const bookmarks::BookmarkNode* node,
                              std::unique_ptr<PowerBookmarkMeta> meta) {
  if (!model || !node)
    return;

  CHECK(meta);

  BackfillShoppingSpecifics(meta.get());

  std::string data;
  EncodeMetaForStorage(*meta.get(), &data);
  model->SetNodeMetaInfo(node, kPowerBookmarkMetaKey, data);
}

void DeleteNodePowerBookmarkMeta(bookmarks::BookmarkModel* model,
                                 const bookmarks::BookmarkNode* node) {
  if (model && node)
    model->DeleteNodeMetaInfo(node, kPowerBookmarkMetaKey);
}

bool DoBookmarkTagsContainWords(const std::unique_ptr<PowerBookmarkMeta>& meta,
                                const std::vector<std::u16string>& words) {
  if (!meta)
    return false;

  for (const std::u16string& word : words) {
    bool found_tag_for_word = false;
    for (const PowerBookmarkMeta_Tag& tag : meta->tags()) {
      if (base::i18n::StringSearchIgnoringCaseAndAccents(
              word, base::UTF8ToUTF16(tag.display_name()), nullptr, nullptr)) {
        found_tag_for_word = true;
        break;
      }
    }
    if (!found_tag_for_word)
      return false;
  }

  return true;
}

template <class type>
std::vector<const bookmarks::BookmarkNode*> GetBookmarksMatchingPropertiesImpl(
    type& iterator,
    bookmarks::BookmarkModel* model,
    const PowerBookmarkQueryFields& query,
    const std::vector<std::u16string>& query_words,
    size_t max_count) {
  std::vector<const bookmarks::BookmarkNode*> nodes;

  while (iterator.has_next()) {
    const bookmarks::BookmarkNode* node = iterator.Next();

    // The query words are allowed to be empty for search.
    bool title_or_url_match_query =
        query_words.empty() || bookmarks::DoesBookmarkContainWords(
                                   node->GetTitle(), node->url(), query_words);

    std::unique_ptr<PowerBookmarkMeta> meta =
        GetNodePowerBookmarkMeta(model, node);

    // Similarly, if the query is empty, we want this test to pass.
    bool tags_match_query =
        query_words.empty() || DoBookmarkTagsContainWords(meta, query_words);

    // Add vertical-specific support by adding a case below.
    if (query.type.has_value()) {
      if (!meta)
        continue;

      bool type_present = false;
      switch (query.type.value()) {
        case PowerBookmarkType::SHOPPING:
          type_present = meta->has_shopping_specifics();
          break;
        default:
          NOTREACHED_IN_MIGRATION();
      }

      if (!type_present)
        continue;
    }

    if (!title_or_url_match_query && !tags_match_query)
      continue;

    if (model->is_permanent_node(node))
      continue;

    if (query.title && node->GetTitle() != *query.title)
      continue;

    // If the tag list is empty, don't try to filter by tag.
    if (!query.tags.empty()) {
      if (!meta)
        continue;

      // TODO(mdjones): This tag comparison can be faster in a lot of ways.
      //                We'll use the simple version while experimenting with
      //                the feature.
      bool found_tag = false;
      for (const std::u16string& tag : query.tags) {
        found_tag = false;
        for (const PowerBookmarkMeta_Tag& proto_tag : meta->tags()) {
          if (base::EqualsCaseInsensitiveASCII(
                  tag, base::UTF8ToUTF16(proto_tag.display_name()))) {
            found_tag = true;
            break;
          }
        }
        if (!found_tag)
          break;
      }

      // if there was a missing tag, skip this bookmark node.
      if (!found_tag)
        continue;
    }

    nodes.push_back(node);
    if (nodes.size() == max_count) {
      break;
    }
  }

  return nodes;
}

std::vector<const bookmarks::BookmarkNode*> GetBookmarksMatchingProperties(
    bookmarks::BookmarkModel* model,
    const PowerBookmarkQueryFields& query,
    size_t max_count) {
  // ParseBookmarkQuery and some of the other util methods come from
  // bookmark_utils.
  std::vector<std::u16string> query_words =
      bookmarks::ParseBookmarkQuery(query);
  if (query.word_phrase_query && query_words.empty() && query.tags.empty()) {
    return {};
  }

  const bookmarks::BookmarkNode* search_folder = model->root_node();
  if (query.folder && query.folder->is_folder())
    search_folder = query.folder;

  if (query.url) {
    // Shortcut into the BookmarkModel if searching for URL.
    GURL url(*query.url);
    std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
        url_matched_nodes;
    if (url.is_valid()) {
      url_matched_nodes = model->GetNodesByURL(url);
    }
    bookmarks::VectorIterator iterator(&url_matched_nodes);
    return GetBookmarksMatchingPropertiesImpl<bookmarks::VectorIterator>(
        iterator, model, query, query_words, max_count);
  }

  ui::TreeNodeIterator<const bookmarks::BookmarkNode> iterator(search_folder);
  return GetBookmarksMatchingPropertiesImpl<
      ui::TreeNodeIterator<const bookmarks::BookmarkNode>>(
      iterator, model, query, query_words, max_count);
}

void EncodeMetaForStorage(const PowerBookmarkMeta& meta, std::string* out) {
  std::string data;
  meta.SerializeToString(&data);
  *out = base::Base64Encode(data);
}

bool DecodeMetaFromStorage(const std::string& data, PowerBookmarkMeta* out) {
  if (!out)
    return false;

  std::string decoded_data;

  if (!base::Base64Decode(data, &decoded_data))
    return false;

  return out->ParseFromString(decoded_data);
}

}  // namespace power_bookmarks
