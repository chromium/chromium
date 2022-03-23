// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/browser/user_note_instance.h"

namespace user_notes {

UserNoteInstance::UserNoteInstance(base::SafeRef<UserNote> model)
    : model_(model) {}

UserNoteInstance::~UserNoteInstance() = default;

}  // namespace user_notes
