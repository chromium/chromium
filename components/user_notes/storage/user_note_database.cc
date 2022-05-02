// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

namespace user_notes {

namespace {

constexpr base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("UserNotes.db");

}  // namespace

UserNoteDatabase::UserNoteDatabase(const base::FilePath& path_to_database_dir)
    : db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 128}),
      db_file_path_(path_to_database_dir.Append(kDatabaseName)) {}

UserNoteDatabase::~UserNoteDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void UserNoteDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

UserNoteMetadataSnapshot UserNoteDatabase::GetNoteMetadataForUrls(
    std::vector<GURL> urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.

  return UserNoteMetadataSnapshot();
}

std::vector<std::unique_ptr<UserNote>> UserNoteDatabase::GetNotesById(
    std::vector<base::UnguessableToken> ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.

  return std::vector<std::unique_ptr<UserNote>>();
}

void UserNoteDatabase::CreateNote(base::UnguessableToken id,
                                  std::string note_body_text,
                                  UserNoteTarget::TargetType target_type,
                                  std::string original_text,
                                  GURL target_page,
                                  std::string selector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::UpdateNote(base::UnguessableToken id,
                                  std::string note_body_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteNote(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteAllForUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteAllForOrigin(const url::Origin& page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteAllNotes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

}  // namespace user_notes
