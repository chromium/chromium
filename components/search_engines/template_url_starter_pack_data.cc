// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url_starter_pack_data.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/search_engines/search_engine_type.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_data_util.h"
#include "components/strings/grit/components_strings.h"

namespace template_url_starter_pack_data {

// Update this whenever a change is made to any starter pack data.
const int kCurrentDataVersion = 13;

// Only update this if there's an incompatible change that requires force
// updating the user's starter pack data. This will overwrite any of the
// user's changes to the starter pack entries.
const int kFirstCompatibleDataVersion = 10;

const StarterPackEngine bookmarks = {
    .name_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_BOOKMARKS_NAME,
    .keyword_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_BOOKMARKS_KEYWORD,
    .favicon_url = nullptr,
    .search_url = "chrome://bookmarks/?q={searchTerms}",
    .destination_url = "chrome://bookmarks",
    .id = StarterPackId::kBookmarks,
    .type = SEARCH_ENGINE_STARTER_PACK_BOOKMARKS,
};

const StarterPackEngine history = {
    .name_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_HISTORY_NAME,
    .keyword_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_HISTORY_KEYWORD,
    .favicon_url = nullptr,
    .search_url = "chrome://history/?q={searchTerms}",
    .destination_url = "chrome://history",
    .id = StarterPackId::kHistory,
    .type = SEARCH_ENGINE_STARTER_PACK_HISTORY,
};

const StarterPackEngine tabs = {
    .name_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_TABS_NAME,
    .keyword_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_TABS_KEYWORD,
    .favicon_url = nullptr,
    // This search_url is a placeholder URL to make templateURL happy.
    // chrome://tabs does not currently exist and the tab search engine will
    // only provide suggestions from the OpenTabProvider.
    .search_url = "chrome://tabs/?q={searchTerms}",
    .destination_url = "http://support.google.com/chrome/?p=tab_search",
    .id = StarterPackId::kTabs,
    .type = SEARCH_ENGINE_STARTER_PACK_TABS,
};

const StarterPackEngine gemini = {
    .name_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_GEMINI_NAME,
    .keyword_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_GEMINI_KEYWORD,
    .favicon_url = nullptr,
    .search_url = "https://gemini.google.com/app?q={searchTerms}",
    .destination_url = "https://gemini.google.com",
    .id = StarterPackId::kGemini,
    .type = SEARCH_ENGINE_STARTER_PACK_GEMINI,
};

const StarterPackEngine page = {
    .name_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_PAGE_NAME,
    .keyword_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_PAGE_KEYWORD,
    .favicon_url = nullptr,
    .search_url = "chrome://page/?q={searchTerms}",
    .destination_url = "chrome://page",
    .id = StarterPackId::kPage,
    .type = SEARCH_ENGINE_STARTER_PACK_PAGE,
};

const StarterPackEngine ai_mode = {
    .name_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_AI_MODE_NAME,
    .keyword_message_id = IDS_SEARCH_ENGINES_STARTER_PACK_AI_MODE_KEYWORD,
    .favicon_url = nullptr,
    // - `udm=50` triggers AI mode as opposed to traditional search.
    // - `aep=48` identifies source of the request as the omnibox as opposed to
    //    e.g. the NTP realbox.
    .search_url =
        "https://www.google.com/"
        "search?sourceid=chrome&udm=50&aep=48&q={searchTerms}",
    .destination_url = "https://www.google.com",
    .id = StarterPackId::kAiMode,
    .type = SEARCH_ENGINE_STARTER_PACK_AI_MODE,
};

const StarterPackEngine* engines[] = {
    &bookmarks, &history, &tabs, &gemini, &page, &ai_mode,
};

int GetDataVersion() {
  return kCurrentDataVersion;
}

int GetFirstCompatibleDataVersion() {
  return kFirstCompatibleDataVersion;
}

std::vector<std::unique_ptr<TemplateURLData>> GetStarterPackEngines() {
  std::vector<std::unique_ptr<TemplateURLData>> t_urls;

  for (auto* engine : engines) {
    t_urls.push_back(TemplateURLDataFromStarterPackEngine(*engine));
  }
  return t_urls;
}

std::u16string GetDestinationUrlForStarterPackId(int id) {
  for (auto* engine : engines) {
    if (engine->id == id) {
      return base::UTF8ToUTF16(engine->destination_url);
    }
  }

  return u"";
}

}  // namespace template_url_starter_pack_data
