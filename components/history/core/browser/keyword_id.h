// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_ID_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_ID_H_

#include <stdint.h>

#include "components/search_engines/template_url_id.h"

namespace history {

// ID of a keyword associated with a URL and a search term.
// 0 is the invalid value, i.e., kInvalidTemplateURLID.
using KeywordID = TemplateURLID;

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_ID_H_
