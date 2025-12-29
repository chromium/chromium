// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

class UnexportableKeysInternalsUI;

// The WebUIConfig for chrome://unexportable-keys-internals.
class UnexportableKeysInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<UnexportableKeysInternalsUI> {
 public:
  UnexportableKeysInternalsUIConfig()
      : content::DefaultInternalWebUIConfig<UnexportableKeysInternalsUI>(
            chrome::kChromeUIUnexportableKeysInternalsHost) {}

  // content::WebUIConfig:
  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

// The WebUIController for chrome://unexportable-keys-internals.
class UnexportableKeysInternalsUI : public content::WebUIController {
 public:
  explicit UnexportableKeysInternalsUI(content::WebUI* web_ui);
  UnexportableKeysInternalsUI(const UnexportableKeysInternalsUI&) = delete;
  UnexportableKeysInternalsUI& operator=(const UnexportableKeysInternalsUI&) =
      delete;
  ~UnexportableKeysInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_UNEXPORTABLE_KEYS_INTERNALS_UNEXPORTABLE_KEYS_INTERNALS_UI_H_
