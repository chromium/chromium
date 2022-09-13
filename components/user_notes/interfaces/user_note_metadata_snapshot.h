// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_METADATA_SNAPSHOT_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_METADATA_SNAPSHOT_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/unguessable_token.h"
#include "url/gurl.h"

namespace user_notes {

class UserNoteMetadata;

// In order to have GURL as a key in a hashmap, GURL hashing mechanism is
// needed.
struct GURLHash {
  size_t operator()(const GURL& url) const {
    return std::hash<std::string>()(url.spec());
  }
};

// A class that encapsulates an
// `unordered_map<GURL, unordered_map<ID, UserNoteMetadata>>`. This represents
// a snapshot of the note metadata contained in the database for a set of URLs.
// The first map is to group metadata by URL, which makes it easy to look up
// what notes are attached to that URL. The second map is for quick lookup of a
// note's metadata by its ID. Using this class makes code simpler and clearer
// than if using the raw type.
class UserNoteMetadataSnapshot {
 public:
  using IdToMetadataMap = std::unordered_map<base::UnguessableToken,
                                             std::unique_ptr<UserNoteMetadata>,
                                             base::UnguessableTokenHash>;
  using UrlToIdToMetadataMap =
      std::unordered_map<GURL, IdToMetadataMap, GURLHash>;

  UserNoteMetadataSnapshot();
  UserNoteMetadataSnapshot(UserNoteMetadataSnapshot&& other);
  UserNoteMetadataSnapshot(const UserNoteMetadataSnapshot&) = delete;
  UserNoteMetadataSnapshot& operator=(const UserNoteMetadataSnapshot&) = delete;
  ~UserNoteMetadataSnapshot();

  // Returns false if there's at least one entry in the snapshot, true
  // otherwise.
  bool IsEmpty();

  // Adds a metadata entry to this class, based on the URL the note is attached
  // to and its ID.
  void AddEntry(const GURL& url,
                const base::UnguessableToken& id,
                std::unique_ptr<UserNoteMetadata> metadata);

  // Returns a raw pointer to the Note ID -> Metadata hash map for the given
  // URL, or nullptr if the URL does not have any notes associated with it.
  const IdToMetadataMap* GetMapForUrl(const GURL& url) const;

 private:
  UrlToIdToMetadataMap url_map_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_METADATA_SNAPSHOT_H_
