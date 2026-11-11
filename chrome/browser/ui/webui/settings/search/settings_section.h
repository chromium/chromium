// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTION_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/settings/search/mojom/setting_id.mojom.h"

namespace settings_search {

class SettingsSearchItem;
class SettingsSearchRegistry;

// Splits a comma-separated synonym list into individual search terms.
// Translators may delimit the list with the Arabic ('،'), ideographic ('、')
// or fullwidth ('，') comma instead of ',', so splits on all of them.
std::vector<std::u16string> SplitSynonyms(std::u16string_view synonyms);

// Splits the comma-separated synonym list stored in the string resource
// `message_id` into individual search terms.
std::vector<std::u16string> GetSynonyms(int message_id);

// Makes an item for a control on the settings page or subpage `sub_page`, its
// route path without the leading slash, e.g. chrome::kAppearanceSubPage.
// Resolves its title, its breadcrumbs (the titles of the pages that contain
// it, outermost first) and, if given, its synonyms from string resources.
std::unique_ptr<SettingsSearchItem> MakeSearchItem(
    mojom::SettingId id,
    std::string_view sub_page,
    int title_id,
    base::span<const int> breadcrumb_ids,
    std::optional<int> synonyms_id = std::nullopt);

// Base class for one settings page. Subclasses register their initial
// searchable settings in their constructors, and add or remove items as
// runtime state changes (prefs, policy, sign-in). SettingsSearchItem takes
// localized strings, so subclasses normally build items with MakeSearchItem(),
// which resolves them; an item whose title needs substitutions is constructed
// directly. Sections need not remove their items on destruction; the registry
// is destroyed with them.
class SettingsSection {
 public:
  // `registry` must outlive `this`.
  explicit SettingsSection(SettingsSearchRegistry* registry);
  SettingsSection(const SettingsSection&) = delete;
  SettingsSection& operator=(const SettingsSection&) = delete;
  virtual ~SettingsSection();

 protected:
  SettingsSearchRegistry* registry() const { return registry_; }

 private:
  const raw_ptr<SettingsSearchRegistry> registry_;
};

}  // namespace settings_search

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_SEARCH_SETTINGS_SECTION_H_
