// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTE_DATA_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTE_DATA_H_

#include <string>

namespace content_creation {

// Struct containing the data of a note.
struct NoteData {
  NoteData(std::string comment,
           std::string quote,
           std::string webpage_url,
           std::string highlight_directive);
  NoteData(NoteData const& note_data);
  ~NoteData();

  std::string comment;
  std::string quote;
  std::string webpage_url;
  std::string highlight_directive;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTE_DATA_H_
