// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_CONFIG_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_CONFIG_H_

#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace feed {

class FeedUIConfig : public content::WebUIConfig {
 public:
  FeedUIConfig();
  ~FeedUIConfig() override = default;

  std::unique_ptr<content::WebUIController> CreateWebUIController(
      content::WebUI* web_ui) override;

  bool IsWebUIEnabled(content::BrowserContext* browser_context) override;
};

}  // namespace feed
#endif  // CHROME_BROWSER_UI_WEBUI_FEED_FEED_UI_CONFIG_H_
