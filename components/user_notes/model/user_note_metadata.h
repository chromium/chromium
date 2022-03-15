// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_METADATA_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_METADATA_H_

namespace user_notes {

// Model class for a note.
class UserNoteMetadata {
 public:
  explicit UserNoteMetadata();
  ~UserNoteMetadata();
  UserNoteMetadata(const UserNoteMetadata&) = delete;
  UserNoteMetadata& operator=(const UserNoteMetadata&) = delete;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_METADATA_H_
