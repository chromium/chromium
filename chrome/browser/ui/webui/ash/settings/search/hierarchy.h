// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_HIERARCHY_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_HIERARCHY_H_

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_identifier.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"

namespace ash::settings {

class OsSettingsSections;

// Tracks the OS settings page hierarchy. Settings is composed of a group of
// sections containing subpages and/or settings, and this class provides
// metadata for where these subpages/settings reside and what localized strings
// are used to describe them.
//
// A subpage can either be a direct child of a section or can be a nested
// subpage, meaning that its parent is another subpage.
//
// A setting can either be embedded as a direct child of a section or can be
// a child of a subpage. Additionally, some settings appear in multiple places.
// For example, the Wi-Fi on/off toggle appears in both the top-level Network
// section as well as the Wi-Fi subpage. In cases like this, we consider the
// "primary" location as the more-targeted one - in this example, the Wi-Fi
// subpage is the primary location of the toggle since it is more specific to
// Wi-Fi, and the alternate location is the one embedded in the Network section.
class Hierarchy {
 public:
  explicit Hierarchy(const OsSettingsSections* sections);
  Hierarchy(const Hierarchy& other) = delete;
  Hierarchy& operator=(const Hierarchy& other) = delete;
  virtual ~Hierarchy();

  class SectionMetadata {
   public:
    SectionMetadata(chromeos::settings::mojom::Section section,
                    const Hierarchy* hierarchy);
    ~SectionMetadata();

    // Generates a search result for this section, using the canonical search
    // tag as the search result text. |relevance_score| must be passed by the
    // client, since this result is being created manually instead of via query
    // matching.
    mojom::SearchResultPtr ToSearchResult(double relevance_score) const;

   private:
    chromeos::settings::mojom::Section section_;
    raw_ptr<const Hierarchy> hierarchy_;
  };

  class SubpageMetadata {
   public:
    SubpageMetadata(int name_message_id,
                    chromeos::settings::mojom::Section section,
                    chromeos::settings::mojom::Subpage subpage,
                    mojom::SearchResultIcon icon,
                    mojom::SearchResultDefaultRank default_rank,
                    const std::string& url_path_with_parameters,
                    const Hierarchy* hierarchy);
    ~SubpageMetadata();

    // Generates a search result for this subpage, using the canonical search
    // tag as the search result text. |relevance_score| must be passed by the
    // client, since this result is being created manually instead of via query
    // matching.
    mojom::SearchResultPtr ToSearchResult(double relevance_score) const;

    // The section in which the subpage appears.
    chromeos::settings::mojom::Section section;

    // The parent subpage, if applicable. Only applies to nested subpages.
    std::optional<chromeos::settings::mojom::Subpage> parent_subpage;

   private:
    chromeos::settings::mojom::Subpage subpage_;

    // Message ID corresponding to the localized string used to describe this
    // subpage.
    int name_message_id_;

    // Icon used for this subpage.
    mojom::SearchResultIcon icon_;

    // Default rank; used to order returned results.
    mojom::SearchResultDefaultRank default_rank_;

    // Static URL path, which may need to be modified via
    // |modify_url_callback_|.
    std::string unmodified_url_path_with_parameters_;

    raw_ptr<const Hierarchy> hierarchy_;
  };

  // The location of a setting, which includes its section and, if applicable,
  // its subpage. Some settings are embedded directly into the section and have
  // no associated subpage.
  struct SettingLocation {
    SettingLocation(chromeos::settings::mojom::Section section,
                    std::optional<chromeos::settings::mojom::Subpage> subpage)
        : section(section), subpage(subpage) {}
    ~SettingLocation() = default;
    chromeos::settings::mojom::Section section;
    std::optional<chromeos::settings::mojom::Subpage> subpage;
  };

  struct SettingMetadata {
    explicit SettingMetadata(
        chromeos::settings::mojom::Section primary_section);
    ~SettingMetadata();

    // The primary location, as described above.
    SettingLocation primary;

    // Alternate locations, as described above. Empty if the setting has no
    // alternate location.
    std::vector<SettingLocation> alternates;
  };

  const SectionMetadata& GetSectionMetadata(
      chromeos::settings::mojom::Section section) const;
  const SubpageMetadata& GetSubpageMetadata(
      chromeos::settings::mojom::Subpage subpage) const;
  const SettingMetadata& GetSettingMetadata(
      chromeos::settings::mojom::Setting setting) const;

  // Generates a list of names of the ancestor sections/subpages for |subpage|.
  // The list contains the Settings app name, the section name and, if
  // applicable, parent subpage names. Names returned in this list are all
  // localized string16s which can be displayed in the UI (e.g., as
  // breadcrumbs).
  //
  // Example 1 - Wi-Fi Networks subpage (no parent subpage):
  //                 ["Settings", "Network"]
  // Example 2 - External storage (has parent subpage):
  //                 ["Settings", "Device", "Storage management"]
  std::vector<std::u16string> GenerateAncestorHierarchyStrings(
      chromeos::settings::mojom::Subpage subpage) const;

  // Same as above, but for settings.
  std::vector<std::u16string> GenerateAncestorHierarchyStrings(
      chromeos::settings::mojom::Setting setting) const;

 protected:
  std::unordered_map<chromeos::settings::mojom::Section, SectionMetadata>
      section_map_;
  std::unordered_map<chromeos::settings::mojom::Subpage, SubpageMetadata>
      subpage_map_;
  std::unordered_map<chromeos::settings::mojom::Setting, SettingMetadata>
      setting_map_;

 private:
  class PerSectionHierarchyGenerator;
  friend std::ostream& operator<<(std::ostream& os, const Hierarchy& h);

  // Generates an array with the Settings app name and |section|'s name.
  std::vector<std::u16string> GenerateHierarchyStrings(
      chromeos::settings::mojom::Section section) const;

  virtual std::string ModifySearchResultUrl(
      chromeos::settings::mojom::Section section,
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const;

  raw_ptr<const OsSettingsSections> sections_;  // Owned by |OsSettingsManager|
};

#ifdef DCHECK
// For logging use only. Prints out text representation of the `Hierarchy`.
std::ostream& operator<<(std::ostream& os, const Hierarchy& h);
#endif

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_SEARCH_HIERARCHY_H_
