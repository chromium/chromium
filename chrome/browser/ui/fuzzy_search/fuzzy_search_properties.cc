// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fuzzy_search/fuzzy_search_properties.h"

namespace fuzzy_search {

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(std::vector<std::u16string>, kSynonymsKey)

}  // namespace fuzzy_search

DEFINE_UI_CLASS_PROPERTY_TYPE(std::vector<std::u16string>*)
