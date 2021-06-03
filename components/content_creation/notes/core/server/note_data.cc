// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/note_data.h"

namespace content_creation {

NoteData::NoteData(std::string comment,
                   std::string quote,
                   std::string webpage_url,
                   std::string highlight_directive)
    : comment(comment),
      quote(quote),
      webpage_url(webpage_url),
      highlight_directive(highlight_directive) {}

NoteData::NoteData(NoteData const& note_data) = default;

NoteData::~NoteData() {}

}  // namespace content_creation
