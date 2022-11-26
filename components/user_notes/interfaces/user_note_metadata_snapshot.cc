// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"

#include <ostream>

#include "components/user_notes/model/user_note_metadata.h"

namespace user_notes {

UserNoteMetadataSnapshot::UserNoteMetadataSnapshot() = default;

UserNoteMetadataSnapshot::UserNoteMetadataSnapshot(
    UserNoteMetadataSnapshot&& other) = default;

UserNoteMetadataSnapshot::~UserNoteMetadataSnapshot() = default;

bool UserNoteMetadataSnapshot::IsEmpty() {
  return url_map_.size() == 0;
}

void UserNoteMetadataSnapshot::AddEntry(
    const GURL& url,
    const base::UnguessableToken& id,
    std::unique_ptr<UserNoteMetadata> metadata) {
  auto url_entry = url_map_.find(url);

  if (url_entry == url_map_.end()) {
    url_map_.emplace(url, IdToMetadataMap());
    return AddEntry(url, id, std::move(metadata));
  }

  DCHECK(url_entry->second.find(id) == url_entry->second.end())
      << "Attempted to add metadata for a note ID twice";
  url_entry->second.emplace(id, std::move(metadata));
}

const UserNoteMetadataSnapshot::IdToMetadataMap*
UserNoteMetadataSnapshot::GetMapForUrl(const GURL& url) const {
  auto url_entry = url_map_.find(url);
  if (url_entry == url_map_.end()) {
    return nullptr;
  } else {
    return &url_entry->second;
  }
}

}  // namespace user_notes
