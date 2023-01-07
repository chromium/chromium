// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_SAVE_NOTE_RESPONSE_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_SAVE_NOTE_RESPONSE_H_

#include <string>

namespace content_creation {

// A struct holding the server response when saving a note.
struct SaveNoteResponse {
  std::string account_id;
  std::string note_id;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_SAVE_NOTE_RESPONSE_H_
