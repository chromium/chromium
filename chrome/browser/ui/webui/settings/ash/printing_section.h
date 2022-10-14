// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PRINTING_SECTION_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PRINTING_SECTION_H_

#include "base/values.h"
#include "chrome/browser/ash/printing/cups_printers_manager.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_section.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace ash::settings {

class SearchTagRegistry;

// Provides UI strings and search tags for Printing settings.
class PrintingSection : public OsSettingsSection,
                        public CupsPrintersManager::Observer {
 public:
  PrintingSection(Profile* profile,
                  SearchTagRegistry* search_tag_registry,
                  CupsPrintersManager* printers_manager);
  ~PrintingSection() override;

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

  // CupsPrintersManager::Observer
  void OnPrintersChanged(
      chromeos::PrinterClass printer_class,
      const std::vector<chromeos::Printer>& printers) override;

  void UpdateSavedPrintersSearchTags();

  CupsPrintersManager* printers_manager_;
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_ASH_PRINTING_SECTION_H_
