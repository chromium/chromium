// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_UI_H_

#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

class LegionInternalsUI;

class LegionInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<LegionInternalsUI> {
 public:
  LegionInternalsUIConfig();

  ~LegionInternalsUIConfig() override;
};

// The WebUI for chrome://legion-internals.
class LegionInternalsUI : public content::WebUIController {
 public:
  explicit LegionInternalsUI(content::WebUI* web_ui);

  LegionInternalsUI(const LegionInternalsUI&) = delete;
  LegionInternalsUI& operator=(const LegionInternalsUI&) = delete;

  ~LegionInternalsUI() override;

 private:
  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_LEGION_INTERNALS_LEGION_INTERNALS_UI_H_
