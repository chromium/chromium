// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_PROPERTIES_H_
#define CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_PROPERTIES_H_

#include <string>
#include <vector>

#include "ui/base/class_property.h"

namespace fuzzy_search {

// Class property key for alternative search terms/keywords (synonyms).
extern const ui::ClassProperty<std::vector<std::u16string>*>* const
    kSynonymsKey;

}  // namespace fuzzy_search

DECLARE_UI_CLASS_PROPERTY_TYPE(std::vector<std::u16string>*)

#endif  // CHROME_BROWSER_UI_FUZZY_SEARCH_FUZZY_SEARCH_PROPERTIES_H_
