// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

class ContentAnnotatorInternalsUI;

// The WebUIConfig for chrome://content-annotator-internals
class ContentAnnotatorInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<ContentAnnotatorInternalsUI> {
 public:
  ContentAnnotatorInternalsUIConfig()
      : content::DefaultInternalWebUIConfig<ContentAnnotatorInternalsUI>(
            chrome::kChromeUIContentAnnotatorInternalsHost) {}
};

// The WebUIController for chrome://content-annotator-internals
class ContentAnnotatorInternalsUI : public content::WebUIController {
 public:
  explicit ContentAnnotatorInternalsUI(content::WebUI* web_ui);
  ContentAnnotatorInternalsUI(const ContentAnnotatorInternalsUI&) = delete;
  ContentAnnotatorInternalsUI& operator=(const ContentAnnotatorInternalsUI&) =
      delete;
  ~ContentAnnotatorInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONTENT_ANNOTATOR_INTERNALS_CONTENT_ANNOTATOR_INTERNALS_UI_H_
