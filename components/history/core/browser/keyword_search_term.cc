// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/keyword_search_term.h"

namespace history {

KeywordSearchTermVisit::KeywordSearchTermVisit() : visits(0) {}

KeywordSearchTermVisit::~KeywordSearchTermVisit() {}


KeywordSearchTermRow::KeywordSearchTermRow() : keyword_id(0), url_id(0) {}

KeywordSearchTermRow::KeywordSearchTermRow(const KeywordSearchTermRow& other) =
    default;

KeywordSearchTermRow::~KeywordSearchTermRow() {}

}  // namespace history
