// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/model/user_note_target.h"

namespace user_notes {

UserNoteTarget::UserNoteTarget(TargetType type,
                               const std::u16string& original_text,
                               const GURL& target_page,
                               const std::string& selector)
    : type_(type),
      original_text_(original_text),
      target_page_(target_page),
      selector_(selector) {}

UserNoteTarget::~UserNoteTarget() = default;

}  // namespace user_notes
