// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_STORAGE_IMPL_H_
#define COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_STORAGE_IMPL_H_

#include <unordered_map>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
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

  using Observer = UserNoteStorage::Observer;

  // Implement UserNoteStorage
  void AddObserver(Observer* observer) override;

  void RemoveObserver(Observer* observer) override;

  void GetNoteMetadataForUrls(
      const UserNoteStorage::UrlSet& urls,
      base::OnceCallback<void(UserNoteMetadataSnapshot)> callback) override;

  void GetNotesById(
      const UserNoteStorage::IdSet& ids,
      base::OnceCallback<void(std::vector<std::unique_ptr<UserNote>>)> callback)
      override;

  void UpdateNote(const UserNote* model,
                  std::u16string note_body_text,
                  bool is_creation = false) override;

  void DeleteNote(const base::UnguessableToken& id) override;

  void DeleteAllForUrl(const GURL& url) override;

  void DeleteAllForOrigin(const url::Origin& origin) override;

  void DeleteAllNotes() override;

 private:
  void OnNotesChanged(bool notes_changed);
  base::ObserverList<Observer>::Unchecked observers_;

  // Owns and manages access to the UserNotesDatabase living on a different
  // sequence.
  base::SequenceBound<UserNoteDatabase> database_;

  base::WeakPtrFactory<UserNoteStorageImpl> weak_factory_{this};
};
}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_STORAGE_IMPL_H_
