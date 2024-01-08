// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_ICON_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_ICON_UTILS_H_

#include <string>
#include <string_view>

// Returns the path of the icon for the search engine, or the empty string if
// not found. Search engines prepopulated in EEA countries are guaranteed to
// have an icon.
// The definition of this function is generated in `generated_icon_utils.cc` by
// the script `tools/search_engine_choice/generate_search_engine_icons.py`.
std::string_view GetSearchEngineGeneratedIconPath(
    const std::u16string& engine_keyword);

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCH_ENGINE_CHOICE_ICON_UTILS_H_
