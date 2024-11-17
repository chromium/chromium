// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class IdentityInternalsUI;

class IdentityInternalsUIConfig
    : public content::DefaultWebUIConfig<IdentityInternalsUI> {
 public:
  IdentityInternalsUIConfig();
};

// The WebUI for chrome://identity-internals
class IdentityInternalsUI
    : public content::WebUIController {
 public:
  explicit IdentityInternalsUI(content::WebUI* web_ui);

  IdentityInternalsUI(const IdentityInternalsUI&) = delete;
  IdentityInternalsUI& operator=(const IdentityInternalsUI&) = delete;

  ~IdentityInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_IDENTITY_INTERNALS_UI_H_
