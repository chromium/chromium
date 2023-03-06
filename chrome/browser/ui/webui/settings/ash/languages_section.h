// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_LANGUAGES_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_LANGUAGES_SECTION_H_

#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Languages & Input settings. Search
// tags for some input features (e.g., Smart Inputs) are used only when
// the relevant features are enabled.
class LanguagesSection : public OsSettingsSection,
                         public input_method::InputMethodManager::Observer {
 public:
  LanguagesSection(Profile* profile,
                   SearchTagRegistry* search_tag_registry,
                   PrefService* pref_service);
  ~LanguagesSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  bool IsEmojiSuggestionAllowed() const;
  bool IsSpellCheckEnabled() const;
  void UpdateSpellCheckSearchTags();

  // input_method::InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  // Used to monitor input method changes.
  base::ScopedObservation<input_method::InputMethodManager,
                          input_method::InputMethodManager::Observer>
      observation_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_LANGUAGES_SECTION_H_
