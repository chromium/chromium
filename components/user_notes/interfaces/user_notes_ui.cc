// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/interfaces/user_notes_ui.h"

namespace user_notes {

// UserNotesUI must implement the user data key for its subclasses, to ensure
// their instances can be retrieved in an implementation-agnostic way.
const int UserNotesUI::kUserDataKey;

}  // namespace user_notes
