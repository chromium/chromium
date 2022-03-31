// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/model/user_note_body.h"

namespace user_notes {

UserNoteBody::UserNoteBody(const std::string& plain_text_value)
    : plain_text_value_(plain_text_value) {}

UserNoteBody::~UserNoteBody() = default;

}  // namespace user_notes
