// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_STORAGE_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_STORAGE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "base/unguessable_token.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/model/user_note.h"
#include "components/user_notes/model/user_note_metadata.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace user_notes {

// Interface that callers can use to interact with the UserNotes in storage.
class UserNoteStorage {
 public:
  // Observer class for the notes storage. Notifies implementers when the notes
  // have changed on disk so they can update their model.
  class Observer {
    // Called when notes have changed on disk.
    virtual void OnNotesChanged() = 0;
  };

  UserNoteStorage() = default;
  UserNoteStorage(const UserNoteStorage&) = delete;
  UserNoteStorage& operator=(const UserNoteStorage&) = delete;
  virtual ~UserNoteStorage() = default;

  // Fetches all `UserNoteMetadata` entries for the given URLs from disk. The
  // results are returned via `callback`, mapped by URL and by note
  // ID.
  virtual void GetNoteMetadataForUrls(
      std::vector<GURL> urls,
      base::OnceCallback<void(UserNoteMetadataSnapshot)> callback) = 0;

  // Fetches all `UserNotes` corresponding to the given IDs from disk. The
  // results are returned via `callback`.
  virtual void GetNotesById(
      std::vector<base::UnguessableToken> ids,
      base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)>
          callback) = 0;

  // Saves a brand-new note or a modified note to disk.
  virtual void UpdateNote(const UserNote* model,
                          std::string note_body_text,
                          bool is_creation = false) = 0;

  // Deletes a note from disk.
  virtual void DeleteNote(const base::UnguessableToken& guid) = 0;

  // Deletes all note from disk for a given url.
  virtual void DeleteAllForUrl(const GURL& url) = 0;

  // Deletes all note from disk for a given url origin.
  virtual void DeleteAllForOrigin(const url::Origin& origin) = 0;

  // Deletes all notes from disk.
  virtual void DeleteAllNotes() = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_STORAGE_H_
