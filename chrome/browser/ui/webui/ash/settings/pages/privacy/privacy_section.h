// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PRIVACY_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PRIVACY_SECTION_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/auth/legacy_fingerprint_engine.h"
#include "chrome/browser/ui/webui/ash/settings/pages/os_settings_section.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/sync_section.h"
#include "chromeos/ash/components/login/auth/auth_performer.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Privacy settings. Note that some
// search tags are added only for official Google Chrome OS builds.
class PrivacySection : public OsSettingsSection {
 public:
  PrivacySection(Profile* profile,
                 SearchTagRegistry* search_tag_registry,
                 PrefService* pref_service);
  ~PrivacySection() override;

  // OsSettingsSection:
  void AddHandlers(content::WebUI* web_ui) override;
  void AddLoadTimeData(content::WebUIDataSource* html_source) override;
  int GetSectionNameMessageId() const override;
  chromeos::settings::mojom::Section GetSection() const override;
  mojom::SearchResultIcon GetSectionIcon() const override;
  const char* GetSectionPath() const override;
  bool LogMetric(chromeos::settings::mojom::Setting setting,
                 base::Value& value) const override;
  void RegisterHierarchy(HierarchyGenerator* generator) const override;

 private:
  bool AreFingerprintSettingsAllowed();
  void UpdateRemoveFingerprintSearchTags();

  std::optional<SyncSection> sync_subsection_;

  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar fingerprint_pref_change_registrar_;

  AuthPerformer auth_performer_;
  LegacyFingerprintEngine fp_engine_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_PRIVACY_SECTION_H_
