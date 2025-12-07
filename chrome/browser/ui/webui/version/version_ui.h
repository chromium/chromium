// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_UI_H_
#define CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_UI_H_

#include "build/build_config.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/webui_config.h"

class VersionUI;

class VersionUIConfig : public content::DefaultWebUIConfig<VersionUI> {
 public:
  VersionUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIVersionHost) {}
};

// The WebUI handler for chrome://version.
class VersionUI : public content::WebUIController {
 public:
  explicit VersionUI(content::WebUI* web_ui);

  VersionUI(const VersionUI&) = delete;
  VersionUI& operator=(const VersionUI&) = delete;

  ~VersionUI() override;

  // Returns the IDS_* string id for the variation of the processor.
  static int VersionProcessorVariation();

  // Loads a data source with many named details comprising version info.
  // The keys are from version_ui_constants.
  static void AddVersionDetailStrings(content::WebUIDataSource* html_source);

#if !BUILDFLAG(IS_ANDROID)
  // Returns a localized version string suitable for displaying in UI.
  static std::u16string GetAnnotatedVersionStringForUi();
#endif  // !BUILDFLAG(IS_ANDROID)
};

#endif  // CHROME_BROWSER_UI_WEBUI_VERSION_VERSION_UI_H_
