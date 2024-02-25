// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_ACCESSIBILITY_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_ACCESSIBILITY_SECTION_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/tts_controller.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Accessibility settings.
class AccessibilitySection : public OsSettingsSection,
                             public content::VoicesChangedDelegate,
                             public extensions::ExtensionRegistryObserver {
 public:
  AccessibilitySection(Profile* profile,
                       SearchTagRegistry* search_tag_registry,
                       PrefService* pref_service);
  ~AccessibilitySection() override;

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
  // content::VoicesChangedDelegate:
  void OnVoicesChanged() override;

  // extensions::ExtensionRegistryObserver:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;

  void AddSelectToSpeakStrings(content::WebUIDataSource* html_source);
  void UpdateSearchTags();
  void UpdateTextToSpeechVoiceSearchTags();
  void UpdateTextToSpeechEnginesSearchTags();

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_ = nullptr;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_A11Y_ACCESSIBILITY_SECTION_H_
