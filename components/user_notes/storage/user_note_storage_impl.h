// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_STORAGE_IMPL_H_
#define COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_STORAGE_IMPL_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/threading/sequence_bound.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/interfaces/user_note_storage.h"
#include "components/user_notes/model/user_note.h"
#include "components/user_notes/storage/user_note_database.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace user_notes {

// Implements UserNoteStorage interface by passing the database requests to
// UserNotesDatabase on the correct sequence.
class UserNoteStorageImpl : public UserNoteStorage {
 public:
  explicit UserNoteStorageImpl(const base::FilePath& path_to_database_dir);
  ~UserNoteStorageImpl() override;
  UserNoteStorageImpl(const UserNoteStorageImpl& other) = delete;
  UserNoteStorageImpl& operator=(const UserNoteStorageImpl& other) = delete;

  // Implement UserNoteStorage
  void GetNoteMetadataForUrls(
      std::vector<GURL> urls,
      base::OnceCallback<void(UserNoteMetadataSnapshot)> callback) override;

  void GetNotesById(
      std::vector<base::UnguessableToken> ids,
      base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)> callback)
      override;

  void UpdateNote(const UserNote* model,
                  std::string note_body_text,
                  bool is_creation = false) override;

  void DeleteNote(const base::UnguessableToken& id) override;

  void DeleteAllForUrl(const GURL& url) override;

  void DeleteAllForOrigin(const url::Origin& origin) override;

  void DeleteAllNotes() override;

 private:
  // Owns and manages access to the UserNotesDatabase living on a different
  // sequence.
  base::SequenceBound<UserNoteDatabase> database_;
};
}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_STORAGE_IMPL_H_
