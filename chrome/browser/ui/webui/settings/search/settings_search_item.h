// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_ITEM_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_ITEM_H_

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/ui/fuzzy_search/fuzzy_search_item.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"

namespace settings_search {

// One searchable setting in desktop chrome://settings. Implements
// FuzzySearchItem so FuzzyFinder can score it against a query.
class SettingsSearchItem : public FuzzySearchItem {
 public:
  SettingsSearchItem(mojom::SettingId id,
                     std::string route_path,
                     std::u16string title,
                     std::vector<std::u16string> breadcrumbs,
                     std::vector<std::u16string> synonyms);

  SettingsSearchItem(const SettingsSearchItem&) = delete;
  SettingsSearchItem& operator=(const SettingsSearchItem&) = delete;
  ~SettingsSearchItem() override;

  mojom::SettingId id() const { return id_; }
  const std::string& route_path() const { return route_path_; }
  const std::vector<std::u16string>& breadcrumbs() const {
    return breadcrumbs_;
  }

  // FuzzySearchItem:
  const std::u16string& GetTitle() const override;
  // Returns the innermost breadcrumb.
  const std::u16string& GetSecondaryText() const override;
  base::span<const std::u16string> GetSynonyms() const override;

 private:
  mojom::SettingId id_;
  // The settings route that renders the setting, e.g. "/appearance"; see
  // chrome/browser/resources/settings/route.ts.
  std::string route_path_;
  std::u16string title_;
  // Titles of the pages and subpages that contain the setting, outermost
  // first. Already localized, like `title_` and `synonyms_`.
  std::vector<std::u16string> breadcrumbs_;
  std::vector<std::u16string> synonyms_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SEARCH_ITEM_H_
