// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feed/feed_ui_config.h"
#include "chrome/browser/ui/webui/feed/feed_ui.h"
#include "components/feed/feed_feature_list.h"

namespace {
const char kFeedHost[] = "feed";
}

namespace feed {

FeedUIConfig::FeedUIConfig()
    // Set scheme and host.
    : WebUIConfig(content::kChromeUIUntrustedScheme, kFeedHost) {}

std::unique_ptr<content::WebUIController> FeedUIConfig::CreateWebUIController(
    content::WebUI* web_ui) {
  return std::make_unique<FeedUI>(web_ui);
}

bool FeedUIConfig::IsWebUIEnabled(content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(feed::kWebUiFeed);
}

}  // namespace feed
