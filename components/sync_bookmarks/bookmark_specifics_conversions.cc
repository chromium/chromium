// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_bookmarks/bookmark_specifics_conversions.h"

#include <string>
#include <unordered_set>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/favicon/core/favicon_service.h"
#include "components/sync/protocol/sync.pb.h"
#include "ui/gfx/favicon_size.h"
#include "url/gurl.h"

namespace sync_bookmarks {

namespace {

void UpdateBookmarkSpecificsMetaInfo(
    const bookmarks::BookmarkNode::MetaInfoMap* metainfo_map,
    sync_pb::BookmarkSpecifics* bm_specifics) {
  for (const std::pair<std::string, std::string>& pair : *metainfo_map) {
    sync_pb::MetaInfo* meta_info = bm_specifics->add_meta_info();
    meta_info->set_key(pair.first);
    meta_info->set_value(pair.second);
  }
}

// Metainfo entries in |specifics| must have unique keys.
bookmarks::BookmarkNode::MetaInfoMap GetBookmarkMetaInfo(
    const sync_pb::BookmarkSpecifics& specifics) {
  bookmarks::BookmarkNode::MetaInfoMap meta_info_map;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    meta_info_map[meta_info.key()] = meta_info.value();
  }
  DCHECK_EQ(static_cast<size_t>(specifics.meta_info_size()),
            meta_info_map.size());
  return meta_info_map;
}

// Sets the favicon of the given bookmark node from the given specifics.
void SetBookmarkFaviconFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* bookmark_node,
    favicon::FaviconService* favicon_service) {
  DCHECK(bookmark_node);
  DCHECK(favicon_service);

  favicon_service->AddPageNoVisitForBookmark(bookmark_node->url(),
                                             bookmark_node->GetTitle());

  const std::string& icon_bytes_str = specifics.favicon();
  scoped_refptr<base::RefCountedString> icon_bytes(
      new base::RefCountedString());
  icon_bytes->data().assign(icon_bytes_str);

  GURL icon_url(specifics.icon_url());

  if (icon_bytes->size() == 0 && icon_url.is_empty()) {
    // Empty icon URL and no bitmap data means no icon mapping.
    favicon_service->DeleteFaviconMappings({bookmark_node->url()},
                                           favicon_base::IconType::kFavicon);
    return;
  }

  if (icon_url.is_empty()) {
    // WebUI pages such as "chrome://bookmarks/" are missing a favicon URL but
    // they have a favicon. In addition, ancient clients (prior to M25) may not
    // be syncing the favicon URL. If the icon URL is not synced, use the page
    // URL as a fake icon URL as it is guaranteed to be unique.
    icon_url = GURL(bookmark_node->url());
  }

  // The client may have cached the favicon at 2x. Use MergeFavicon() as not to
  // overwrite the cached 2x favicon bitmap. Sync favicons are always
  // gfx::kFaviconSize in width and height. Store the favicon into history
  // as such.
  gfx::Size pixel_size(gfx::kFaviconSize, gfx::kFaviconSize);
  favicon_service->MergeFavicon(bookmark_node->url(), icon_url,
                                favicon_base::IconType::kFavicon, icon_bytes,
                                pixel_size);
}

}  // namespace

sync_pb::EntitySpecifics CreateSpecificsFromBookmarkNode(
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    bool force_favicon_load) {
  sync_pb::EntitySpecifics specifics;
  sync_pb::BookmarkSpecifics* bm_specifics = specifics.mutable_bookmark();
  if (!node->is_folder()) {
    bm_specifics->set_url(node->url().spec());
  }
  bm_specifics->set_title(base::UTF16ToUTF8(node->GetTitle()));
  bm_specifics->set_creation_time_us(
      node->date_added().ToDeltaSinceWindowsEpoch().InMicroseconds());

  if (node->GetMetaInfoMap()) {
    UpdateBookmarkSpecificsMetaInfo(node->GetMetaInfoMap(), bm_specifics);
  }

  if (!force_favicon_load && !node->is_favicon_loaded()) {
    return specifics;
  }

  // Encodes a bookmark's favicon into raw PNG data.
  scoped_refptr<base::RefCountedMemory> favicon_bytes(nullptr);
  const gfx::Image& favicon = model->GetFavicon(node);
  // Check for empty images. This can happen if the favicon is still being
  // loaded. Also avoid syncing touch icons.
  if (!favicon.IsEmpty() &&
      model->GetFaviconType(node) == favicon_base::IconType::kFavicon) {
    // TODO(crbug.com/516866): Verify that this isn't  problematic for bookmarks
    // created on iOS devices.

    // Re-encode the BookmarkNode's favicon as a PNG.
    favicon_bytes = favicon.As1xPNGBytes();
  }

  if (favicon_bytes.get() && favicon_bytes->size() != 0) {
    bm_specifics->set_favicon(favicon_bytes->front(), favicon_bytes->size());
    bm_specifics->set_icon_url(node->icon_url() ? node->icon_url()->spec()
                                                : std::string());
  } else {
    bm_specifics->clear_favicon();
    bm_specifics->clear_icon_url();
  }

  return specifics;
}

const bookmarks::BookmarkNode* CreateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* parent,
    int index,
    bool is_folder,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service) {
  DCHECK(parent);
  DCHECK(model);
  DCHECK(favicon_service);

  bookmarks::BookmarkNode::MetaInfoMap metainfo =
      GetBookmarkMetaInfo(specifics);
  const bookmarks::BookmarkNode* node;
  if (is_folder) {
    node = model->AddFolderWithMetaInfo(
        parent, index, base::UTF8ToUTF16(specifics.title()), &metainfo);
  } else {
    const int64_t create_time_us = specifics.creation_time_us();
    base::Time create_time = base::Time::FromDeltaSinceWindowsEpoch(
        // Use FromDeltaSinceWindowsEpoch because create_time_us has
        // always used the Windows epoch.
        base::TimeDelta::FromMicroseconds(create_time_us));
    node = model->AddURLWithCreationTimeAndMetaInfo(
        parent, index, base::UTF8ToUTF16(specifics.title()),
        GURL(specifics.url()), create_time, &metainfo);
  }
  if (node) {
    SetBookmarkFaviconFromSpecifics(specifics, node, favicon_service);
  }
  return node;
}

void UpdateBookmarkNodeFromSpecifics(
    const sync_pb::BookmarkSpecifics& specifics,
    const bookmarks::BookmarkNode* node,
    bookmarks::BookmarkModel* model,
    favicon::FaviconService* favicon_service) {
  DCHECK(node);
  DCHECK(model);
  DCHECK(favicon_service);

  if (!node->is_folder()) {
    model->SetURL(node, GURL(specifics.url()));
  }

  model->SetTitle(node, base::UTF8ToUTF16(specifics.title()));
  model->SetNodeMetaInfoMap(node, GetBookmarkMetaInfo(specifics));
  SetBookmarkFaviconFromSpecifics(specifics, node, favicon_service);
}

bool IsValidBookmarkSpecifics(const sync_pb::BookmarkSpecifics& specifics,
                              bool is_folder) {
  if (specifics.ByteSize() == 0) {
    DLOG(ERROR) << "Invalid bookmark: empty specifics.";
    return false;
  }
  if (!is_folder) {
    if (!GURL(specifics.url()).is_valid()) {
      DLOG(ERROR) << "Invalid bookmark: invalid url in the specifics.";
      return false;
    }
    if (specifics.favicon().empty() && !specifics.icon_url().empty()) {
      DLOG(ERROR) << "Invalid bookmark: specifics cannot have an icon_url "
                     "without having a favicon.";
      return false;
    }
    if (!specifics.icon_url().empty() &&
        !GURL(specifics.icon_url()).is_valid()) {
      DLOG(ERROR) << "Invalid bookmark: invalid icon_url in specifics.";
      return false;
    }
  }

  // Verify all keys in meta_info are unique.
  std::unordered_set<base::StringPiece, base::StringPieceHash> keys;
  for (const sync_pb::MetaInfo& meta_info : specifics.meta_info()) {
    if (!keys.insert(meta_info.key()).second) {
      DLOG(ERROR) << "Invalid bookmark: keys in meta_info aren't unique.";
      return false;
    }
  }
  return true;
}

}  // namespace sync_bookmarks
