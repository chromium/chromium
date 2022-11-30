// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTE_DATA_H_
#define COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTE_DATA_H_

#include "url/gurl.h"

#include <string>

namespace content_creation {

// Struct containing the data of a note.
struct NoteData {
  NoteData(std::string comment,
           std::string quote,
           GURL webpage_url,
           std::string highlight_directive);
  NoteData(std::string quote, std::string full_url);
  NoteData(NoteData const& note_data);
  ~NoteData();

  std::string comment;
  std::string quote;
  GURL webpage_url;
  std::string highlight_directive;
};

}  // namespace content_creation

#endif  // COMPONENTS_CONTENT_CREATION_NOTES_CORE_SERVER_NOTE_DATA_H_
