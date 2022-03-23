// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/model/user_note.h"

namespace user_notes {

UserNote::UserNote(const std::string& guid) : guid_(guid) {}

UserNote::~UserNote() = default;

base::SafeRef<UserNote> UserNote::GetSafeRef() {
  return weak_ptr_factory_.GetSafeRef();
}

}  // namespace user_notes
