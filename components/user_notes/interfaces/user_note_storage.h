// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_STORAGE_H_
#define COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_STORAGE_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/callback.h"
#include "components/user_notes/model/user_note.h"
#include "components/user_notes/model/user_note_metadata.h"
#include "url/gurl.h"

namespace user_notes {

// Interface that callers can use to interact with the UserNotes in storage.
class UserNoteStorage {
  typedef std::unordered_map<std::string, std::unique_ptr<UserNoteMetadata>>
      NoteMetadataIdMap;

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

  // Fetches all |UserNoteMetadata| entries for the given URLs from disk. The
  // results are returned via |callback|, mapped by URL and by note ID.
  virtual void GetNoteMetadataForUrls(
      std::vector<GURL> urls,
      base::OnceCallback<void(std::unordered_map<GURL, NoteMetadataIdMap>)>
          callback) = 0;

  // Fetches all |UserNotes| corresponding to the given IDs from disk. The
  // results are returned via |callback|.
  virtual void GetNotesById(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<UserNote>)> callback) = 0;

  // Saves a brand-new note to disk.
  virtual void CreateNote(UserNote note) = 0;

  // Saves a modified note to disk.
  virtual void UpdateNote(UserNote note) = 0;

  // Deletes a note from disk.
  virtual void DeleteNote(std::string guid) = 0;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_INTERFACES_USER_NOTE_STORAGE_H_
