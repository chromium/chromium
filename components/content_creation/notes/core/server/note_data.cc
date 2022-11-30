// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_creation/notes/core/server/note_data.h"

#include "components/shared_highlighting/core/common/fragment_directives_utils.h"
#include "url/gurl.h"

namespace content_creation {

NoteData::NoteData(std::string comment,
                   std::string quote,
                   GURL webpage_url,
                   std::string highlight_directive)
    : comment(std::move(comment)),
      quote(std::move(quote)),
      webpage_url(std::move(webpage_url)),
      highlight_directive(std::move(highlight_directive)) {}

NoteData::NoteData(std::string quote, std::string full_url)
    : quote(std::move(quote)) {
  if (!shared_highlighting::SplitUrlTextFragmentDirective(
          full_url, &webpage_url, &highlight_directive)) {
    webpage_url = GURL(full_url);
  }
}

NoteData::NoteData(NoteData const& note_data) = default;

NoteData::~NoteData() {}

}  // namespace content_creation
