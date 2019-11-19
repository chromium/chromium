// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ENTRY_H_
#define COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ENTRY_H_

#include <string>
#include <vector>

#include "url/gurl.h"

namespace dom_distiller {

struct ArticleEntry {
  ArticleEntry();
  ArticleEntry(const ArticleEntry&);
  ~ArticleEntry();

  std::string entry_id;
  std::string title;
  std::vector<GURL> pages;
};

// A valid entry has a non-empty entry_id and all its pages have a valid URL.
bool IsEntryValid(const ArticleEntry& entry);

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_ARTICLE_ENTRY_H_
