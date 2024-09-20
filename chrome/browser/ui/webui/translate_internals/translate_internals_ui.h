// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_UI_H_

#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

class TranslateInternalsUI;

class TranslateInternalsUIConfig
    : public content::DefaultWebUIConfig<TranslateInternalsUI> {
 public:
  TranslateInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUITranslateInternalsHost) {}
};

// The implementation for the chrome://translate-internals page.
class TranslateInternalsUI : public content::WebUIController {
 public:
  explicit TranslateInternalsUI(content::WebUI* web_ui);

  TranslateInternalsUI(const TranslateInternalsUI&) = delete;
  TranslateInternalsUI& operator=(const TranslateInternalsUI&) = delete;

  ~TranslateInternalsUI() override {}
};

#endif  // CHROME_BROWSER_UI_WEBUI_TRANSLATE_INTERNALS_TRANSLATE_INTERNALS_UI_H_
