// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_INPUTS_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_INPUTS_SECTION_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/input_method/editor_mediator.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/ime/ash/input_method_manager.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Input settings. Search tags for some
// input features (e.g., Suggestions, Edit dictionary) are used only when the
// relevant features are enabled.
class InputsSection : public OsSettingsSection,
                      public input_method::InputMethodManager::Observer {
 public:
  InputsSection(Profile* profile,
                SearchTagRegistry* search_tag_registry,
                PrefService* pref_service,
                input_method::EditorMediator* editor_mediator);
  ~InputsSection() override;

  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

 private:
  bool ShouldShowEmojiSuggestionsSettings() const;
  bool IsSpellCheckEnabled() const;
  void UpdateSpellCheckSearchTags();

  // input_method::InputMethodManager::Observer:
  void InputMethodChanged(input_method::InputMethodManager* manager,
                          Profile* profile,
                          bool show_message) override;

  // Not owned by this class
  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  raw_ptr<input_method::EditorMediator> editor_mediator_;

  PrefChangeRegistrar pref_change_registrar_;

  // Used to monitor input method changes.
  base::ScopedObservation<input_method::InputMethodManager,
                          input_method::InputMethodManager::Observer>
      observation_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_DEVICE_INPUTS_SECTION_H_
