// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/model/user_note.h"

namespace user_notes {

UserNote::UserNote(const base::UnguessableToken& id,
                   std::unique_ptr<UserNoteMetadata> metadata,
                   std::unique_ptr<UserNoteBody> body,
                   std::unique_ptr<UserNoteTarget> target)
    : id_(id),
      metadata_(std::move(metadata)),
      body_(std::move(body)),
      target_(std::move(target)) {}

UserNote::~UserNote() = default;

base::SafeRef<UserNote> UserNote::GetSafeRef() const {
  return weak_ptr_factory_.GetSafeRef();
}

void UserNote::Update(std::unique_ptr<UserNote> new_model) {
  DCHECK(new_model->id() == id_);
  metadata_ = std::move(new_model->metadata_);
  body_ = std::move(new_model->body_);
  target_ = std::move(new_model->target_);
}

}  // namespace user_notes
