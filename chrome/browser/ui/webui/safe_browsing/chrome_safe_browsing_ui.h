// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SAFE_BROWSING_CHROME_SAFE_BROWSING_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SAFE_BROWSING_CHROME_SAFE_BROWSING_UI_H_

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace safe_browsing {

class ChromeSafeBrowsingUI;

class ChromeSafeBrowsingUIConfig
    : public content::DefaultWebUIConfig<ChromeSafeBrowsingUI> {
 public:
  ChromeSafeBrowsingUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           safe_browsing::kChromeUISafeBrowsingHost) {}
};

class ChromeSafeBrowsingUI : public SafeBrowsingUI {
 public:
  explicit ChromeSafeBrowsingUI(content::WebUI* web_ui);

  ChromeSafeBrowsingUI(const ChromeSafeBrowsingUI&) = delete;
  ChromeSafeBrowsingUI& operator=(const ChromeSafeBrowsingUI&) = delete;

  ~ChromeSafeBrowsingUI() override;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_WEBUI_SAFE_BROWSING_CHROME_SAFE_BROWSING_UI_H_
