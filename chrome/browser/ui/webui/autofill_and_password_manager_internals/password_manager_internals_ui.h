// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
}

class PasswordManagerInternalsUI;

class PasswordManagerInternalsUIConfig
    : public content::DefaultWebUIConfig<PasswordManagerInternalsUI> {
 public:
  PasswordManagerInternalsUIConfig();
  ~PasswordManagerInternalsUIConfig() override;
};

class PasswordManagerInternalsUI : public content::WebUIController {
 public:
  explicit PasswordManagerInternalsUI(content::WebUI* web_ui);

  PasswordManagerInternalsUI(const PasswordManagerInternalsUI&) = delete;
  PasswordManagerInternalsUI& operator=(const PasswordManagerInternalsUI&) =
      delete;

  ~PasswordManagerInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_PASSWORD_MANAGER_INTERNALS_UI_H_
