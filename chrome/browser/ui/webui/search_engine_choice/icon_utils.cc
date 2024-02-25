// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"

#include <string_view>

#include "base/containers/fixed_flat_map.h"
#include "build/branding_buildflags.h"

namespace {

// Generated code defining `kSearchEngineIconPathMap`.
#include "chrome/browser/ui/webui/search_engine_choice/generated_icon_utils-inc.cc"

}  // namespace

std::string_view GetSearchEngineGeneratedIconPath(
    const std::u16string& engine_keyword) {
  const base::fixed_flat_map<std::u16string_view, std::string_view,
                             kSearchEngineIconPathMap.size()>::const_iterator
      iterator = kSearchEngineIconPathMap.find(engine_keyword);
  return iterator == kSearchEngineIconPathMap.cend() ? std::string_view()
                                                     : iterator->second;
}
