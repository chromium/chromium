// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_BODY_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_BODY_H_

#include <string>

namespace user_notes {

// Model class for a note body.
class UserNoteBody {
 public:
  enum BodyType { PLAIN_TEXT = 0, RICH_TEXT, IMAGE };

  explicit UserNoteBody(const std::u16string& plain_text_value);
  ~UserNoteBody();
  UserNoteBody(const UserNoteBody&) = delete;
  UserNoteBody& operator=(const UserNoteBody&) = delete;

  BodyType type() const { return type_; }

  const std::u16string& plain_text_value() const { return plain_text_value_; }

 private:
  // The type of body this note has. Currently only plain text is supported.
  BodyType type_ = BodyType::PLAIN_TEXT;

  // The note body in plain text
  std::u16string plain_text_value_;
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_BODY_H_
