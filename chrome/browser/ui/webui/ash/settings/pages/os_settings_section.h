// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_OS_SETTINGS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_OS_SETTINGS_SECTION_H_

#include <string>
#include <vector>

#include "ash/webui/settings/public/constants/routes.mojom.h"
#include "ash/webui/settings/public/constants/setting.mojom.h"
#include "base/containers/span.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_concept.h"

class Profile;

namespace content {
class WebUI;
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Represents one top-level section of the settings app (i.e., one item on the
// settings UI navigation).
//
// When instantiated, an OsSettingsSection should track whether the section and
// any of its subpages or individual settings should be available to the user
// based on the current environment (user account details, device capabilities,
// flags, etc). If a section is available, search tags should be added for that
// section via AddSearchTags(), and if that section becomes unavailable, search
// tags should be removed via RemoveSearchTags().
//
// When the settings app is initialized, this class is used to add loadTimeData
// (e.g., localized strings and flag values) as well as SettingsPageUIHandlers
// (i.e., browser to JS IPC mechanisms) to the page.
class OsSettingsSection {
 public:
  // Used to construct a hierarchy of this section (i.e., which settings and
  // subpages are part of this section). See RegisterHierarchy() below.
  //
  // Note on primary vs. alternate setting locations: Some settings are located
  // in multiple spots of settings. For example, the Wi-Fi on/off toggle appears
  // in both the top-level Network section as well as the Wi-Fi subpage. In
  // cases like this, we consider the "primary" location as the more-targeted
  // one - in this example, the Wi-Fi subpage is the primary location of the
  // toggle since it is more specific to Wi-Fi, and the alternate location is
  // the one embedded in the Network section.
  class HierarchyGenerator {
   public:
    virtual ~HierarchyGenerator() = default;

    // Registers a subpage whose parent is this section.
    virtual void RegisterTopLevelSubpage(
        int name_message_id,
        chromeos::settings::mojom::Subpage subpage,
        mojom::SearchResultIcon icon,
        mojom::SearchResultDefaultRank default_rank,
        const std::string& url_path_with_parameters) = 0;

    // Registers a subpage whose parent is another subpage in this section.
    virtual void RegisterNestedSubpage(
        int name_message_id,
        chromeos::settings::mojom::Subpage subpage,
        chromeos::settings::mojom::Subpage parent_subpage,
        mojom::SearchResultIcon icon,
        mojom::SearchResultDefaultRank default_rank,
        const std::string& url_path_with_parameters) = 0;

    // Registers a setting embedded directly in the section (i.e., not within a
    // subpage). This functions is for primary locations (see above).
    virtual void RegisterTopLevelSetting(
        chromeos::settings::mojom::Setting setting) = 0;

    // Registers a setting embedded within a subpage in this section. This
    // function is for primary locations (see above).
    virtual void RegisterNestedSetting(
        chromeos::settings::mojom::Setting setting,
        chromeos::settings::mojom::Subpage subpage) = 0;

    // Register an alternate location for a setting embedded directly in the
    // section (i.e., not within a subpage). This function is for alternate
    // locations (see above).
    virtual void RegisterTopLevelAltSetting(
        chromeos::settings::mojom::Setting setting) = 0;

    // Registers a setting embedded within a subpage in this section. This
    // function is for alternate locations (see above).
    virtual void RegisterNestedAltSetting(
        chromeos::settings::mojom::Setting setting,
        chromeos::settings::mojom::Subpage subpage) = 0;
  };

  virtual ~OsSettingsSection();

  OsSettingsSection(const OsSettingsSection& other) = delete;
  OsSettingsSection& operator=(const OsSettingsSection& other) = delete;

  // Provides static data (i.e., localized strings and flag values) to an OS
  // settings instance.
  virtual void AddLoadTimeData(content::WebUIDataSource* html_source) = 0;

  // Adds SettingsPageUIHandlers to an OS settings instance. Override if the
  // derived type requires one or more handlers for this section.
  virtual void AddHandlers(content::WebUI* web_ui) {}

  // Provides the message ID for the name of this section.
  virtual int GetSectionNameMessageId() const = 0;

  // Provides the Section enum for this section.
  virtual chromeos::settings::mojom::Section GetSection() const = 0;

  // Provides the icon for this section.
  virtual mojom::SearchResultIcon GetSectionIcon() const = 0;

  // Provides the path for this section. Must return a string constant defined
  // in `routes.mojom`.
  virtual const char* GetSectionPath() const = 0;

  // Logs metrics for the updated |setting| with optional |value|. Returns
  // whether the setting change was logged.
  virtual bool LogMetric(chromeos::settings::mojom::Setting setting,
                         base::Value& value) const = 0;

  // Registers the subpages and/or settings which reside in this section. Every
  // subpage and setting within a section must be registered, regardless of
  // whether or not the subpage or setting is gated behind a feature flag.
  virtual void RegisterHierarchy(HierarchyGenerator* generator) const = 0;

  // Modifies a URL to be used by settings search. Some URLs require dynamic
  // content (e.g., network detail settings use the GUID of the network as a URL
  // parameter to route to details for a specific network). By default, this
  // function simply returns |url_to_modify|, which provides  functionality for
  // static URLs.
  virtual std::string ModifySearchResultUrl(
      mojom::SearchResultType type,
      OsSettingsIdentifier id,
      const std::string& url_to_modify) const;

  // Generates a search result corresponding to this section. |relevance_score|
  // must be passed by the client, since this result is being created manually
  // instead of via query matching.
  mojom::SearchResultPtr GenerateSectionSearchResult(
      double relevance_score) const;

  static std::u16string GetHelpUrlWithBoard(const std::string& original_url);

 protected:
  static void RegisterNestedSettingBulk(
      chromeos::settings::mojom::Subpage,
      const base::span<const chromeos::settings::mojom::Setting>& settings,
      HierarchyGenerator* generator);

  OsSettingsSection(Profile* profile, SearchTagRegistry* search_tag_registry);

  // Used by tests.
  OsSettingsSection();

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }
  SearchTagRegistry* registry() { return search_tag_registry_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(OsSettingsSectionTest, Section);
  FRIEND_TEST_ALL_PREFIXES(OsSettingsSectionTest, Subpage);
  FRIEND_TEST_ALL_PREFIXES(OsSettingsSectionTest, Setting);
  FRIEND_TEST_ALL_PREFIXES(OsSettingsSectionTest, SettingExistingQuery);

  static constexpr char kSettingIdUrlParam[] = "settingId";

  // If type is Setting, adds the kSettingIdUrlParam to the query parameter
  // and returns the deep linked url. Doesn't modify url otherwise.
  static std::string GetDefaultModifiedUrl(mojom::SearchResultType type,
                                           OsSettingsIdentifier id,
                                           const std::string& url_to_modify);

  const raw_ptr<Profile, DanglingUntriaged> profile_ = nullptr;
  const raw_ptr<SearchTagRegistry> search_tag_registry_ = nullptr;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_OS_SETTINGS_SECTION_H_
