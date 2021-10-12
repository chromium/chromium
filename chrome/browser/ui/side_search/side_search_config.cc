// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_search/side_search_config.h"

#include "chrome/common/webui_url_constants.h"
#include "components/google/core/common/google_util.h"
#include "content/public/browser/browser_context.h"
#include "url/gurl.h"

namespace {

constexpr char kSideSearchConfigKey[] = "side_search_config_key";

}  // namespace

SideSearchConfig::SideSearchConfig() = default;

SideSearchConfig::~SideSearchConfig() = default;

// static
SideSearchConfig* SideSearchConfig::Get(content::BrowserContext* context) {
  SideSearchConfig* data = static_cast<SideSearchConfig*>(
      context->GetUserData(kSideSearchConfigKey));
  if (!data) {
    auto new_data = std::make_unique<SideSearchConfig>();
    data = new_data.get();
    context->SetUserData(kSideSearchConfigKey, std::move(new_data));
  }
  return data;
}

bool SideSearchConfig::ShouldNavigateInSidePanel(const GURL& url) {
  return google_util::IsGoogleSearchUrl(url);
}

bool SideSearchConfig::CanShowSidePanelForURL(const GURL& url) {
  return is_side_panel_srp_available_ && !ShouldNavigateInSidePanel(url) &&
         !google_util::IsGoogleHomePageUrl(url) &&
         url.spec() != chrome::kChromeUINewTabURL;
}
