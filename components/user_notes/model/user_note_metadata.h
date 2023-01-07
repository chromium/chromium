// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_METADATA_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_METADATA_H_

#include "base/time/time.h"

namespace user_notes {

// Model class for a note.
class UserNoteMetadata {
 public:
  UserNoteMetadata(base::Time creation_date,
                   base::Time modification_date,
                   int min_note_version);
  ~UserNoteMetadata();
  UserNoteMetadata(const UserNoteMetadata&) = delete;
  UserNoteMetadata& operator=(const UserNoteMetadata&) = delete;

  base::Time creation_date() const { return creation_date_; }
  base::Time modification_date() const { return modification_date_; }
  int min_note_version() const { return min_note_version_; }

 private:
  // The date and time (stored in seconds UTC) when the note was created.
  base::Time creation_date_;

  // The date and time (stored in seconds UTC) when the note was last modified.
  base::Time modification_date_;

  // The minimum User Note version required to support this note.
  int min_note_version_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_METADATA_H_
