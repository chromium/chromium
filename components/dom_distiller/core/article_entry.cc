// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/article_entry.h"

#include "base/logging.h"

namespace dom_distiller {

ArticleEntry::ArticleEntry() = default;
ArticleEntry::ArticleEntry(const ArticleEntry&) = default;
ArticleEntry::~ArticleEntry() = default;

bool IsEntryValid(const ArticleEntry& entry) {
  if (entry.entry_id.empty())
    return false;
  for (const GURL& page : entry.pages) {
    if (!page.is_valid())
      return false;
  }
  return true;
}

}  // namespace dom_distiller
