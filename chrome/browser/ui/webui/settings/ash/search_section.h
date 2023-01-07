// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SEARCH_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SEARCH_SECTION_H_

#include "ash/public/cpp/assistant/assistant_state_base.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chrome/browser/ui/webui/settings/ash/search/search_tag_registry.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace chromeos {
namespace settings {

// Provides UI strings and search tags for Search & Assistant settings. Search
// tags for Assistant settings are added/removed depending on whether the
// feature and relevant flags are enabled/disabled.
class SearchSection : public OsSettingsSection,
                      public ash::AssistantStateObserver,
                      public QuickAnswersStateObserver {
 public:
  SearchSection(Profile* profile, SearchTagRegistry* search_tag_registry);
  ~SearchSection() override;

 private:
  // OsSettingsSection:
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  void AddHandlers(content::WebUI* web_ui) override;
  int GetSectionNameMessageId() const override;
  mojom::Section GetSection() const override;
  ash::settings::mojom::SearchResultIcon GetSectionIcon() const override;
  std::string GetSectionPath() const override;
  bool LogMetric(mojom::Setting setting, base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

  // ash::AssistantStateObserver:
  void OnAssistantConsentStatusChanged(int consent_status) override;
  void OnAssistantContextEnabled(bool enabled) override;
  void OnAssistantSettingsEnabled(bool enabled) override;
  void OnAssistantHotwordEnabled(bool enabled) override;

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool enabled) override;
  void OnEligibilityChanged(bool eligible) override;

  bool IsAssistantAllowed() const;
  void UpdateAssistantSearchTags();
  void UpdateQuickAnswersSearchTags();
};

}  // namespace settings
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when it moved to ash.
namespace ash::settings {
using ::chromeos::settings::SearchSection;
}

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_SEARCH_SECTION_H_
