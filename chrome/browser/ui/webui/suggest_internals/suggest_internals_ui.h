// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_UI_H_

#include "build/build_config.h"
#include "ui/webui/mojo_web_ui_controller.h"

// The Web UI controller for the chrome://suggest-internals.
class SuggestInternalsUI : public ui::MojoWebUIController {
 public:
  explicit SuggestInternalsUI(content::WebUI* web_ui);
  SuggestInternalsUI(const SuggestInternalsUI&) = delete;
  SuggestInternalsUI& operator=(const SuggestInternalsUI&) = delete;
  ~SuggestInternalsUI() override;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SUGGEST_INTERNALS_SUGGEST_INTERNALS_UI_H_
