// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_MODEL_USER_NOTE_H_
#define COMPONENTS_USER_NOTES_MODEL_USER_NOTE_H_

#include <string>

#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "components/user_notes/model/user_note_body.h"
#include "components/user_notes/model/user_note_metadata.h"
#include "components/user_notes/model/user_note_target.h"

namespace user_notes {

// Model class for a note.
class UserNote {
 public:
  static std::unique_ptr<UserNote> Clone(const UserNote* note);

  UserNote(const base::UnguessableToken& id,
           std::unique_ptr<UserNoteMetadata> metadata,
           std::unique_ptr<UserNoteBody> body,
           std::unique_ptr<UserNoteTarget> target);

  ~UserNote();
  UserNote(const UserNote&) = delete;
  UserNote& operator=(const UserNote&) = delete;

  base::SafeRef<UserNote> GetSafeRef() const;

  const base::UnguessableToken& id() const { return id_; }
  const UserNoteMetadata& metadata() const { return *metadata_; }
  const UserNoteBody& body() const { return *body_; }
  const UserNoteTarget& target() const { return *target_; }

  // Consumes the provided model to update this one.
  void Update(std::unique_ptr<UserNote> new_model);

 private:
  // The unique (among the user's notes) ID for this note.
  base::UnguessableToken id_;

  std::unique_ptr<UserNoteMetadata> metadata_;
  std::unique_ptr<UserNoteBody> body_;
  std::unique_ptr<UserNoteTarget> target_;

  base::WeakPtrFactory<UserNote> weak_ptr_factory_{this};
};

}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_MODEL_USER_NOTE_H_
