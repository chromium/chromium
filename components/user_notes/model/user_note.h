// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_H_

#include <string>

namespace user_notes {

// Model class for a note.
class UserNote {
 public:
  explicit UserNote(const std::string& guid);
  ~UserNote();
  UserNote(const UserNote&) = delete;
  UserNote& operator=(const UserNote&) = delete;

  const std::string& guid() const { return guid_; }

 private:
  // The unique (among the user's notes) ID for this note.
  std::string guid_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_H_
