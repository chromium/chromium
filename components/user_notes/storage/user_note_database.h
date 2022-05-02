// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_DATABASE_H_
#define COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_DATABASE_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/model/user_note.h"
#include "sql/database.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace user_notes {

// Provides the backend SQLite support for user notes.
// This class must be used on a same blocking sequence.
class UserNoteDatabase {
 public:
  explicit UserNoteDatabase(const base::FilePath& path_to_database_dir);
  ~UserNoteDatabase();

  // Initialises internal database. Must be called prior to any other usage.
  void Init();

  UserNoteMetadataSnapshot GetNoteMetadataForUrls(std::vector<GURL> urls);

  std::vector<std::unique_ptr<UserNote>> GetNotesById(
      std::vector<base::UnguessableToken> ids);

  void CreateNote(base::UnguessableToken id,
                  std::string note_body_text,
                  UserNoteTarget::TargetType target_type,
                  std::string original_text,
                  GURL target_page,
                  std::string selector);

  void UpdateNote(base::UnguessableToken id, std::string note_body_text);

  void DeleteNote(const base::UnguessableToken& id);

  void DeleteAllForUrl(const GURL& url);

  void DeleteAllForOrigin(const url::Origin& origin);

  void DeleteAllNotes();

 private:
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  const base::FilePath db_file_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_DATABASE_H_
