// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_URL_BUILDER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_URL_BUILDER_H_

#include "base/memory/raw_ptr.h"
#include "url/gurl.h"

class PrefService;
namespace companion {

// Utility to build URL for the search companion request. The URL contains
// various query parameters needed at the server side such as main page URL,
// origin, promo state etc. The params are packed into a single protobuf for
// schema consistency.
class CompanionUrlBuilder {
 public:
  explicit CompanionUrlBuilder(PrefService* pref_service);
  CompanionUrlBuilder(const CompanionUrlBuilder&) = delete;
  CompanionUrlBuilder& operator=(const CompanionUrlBuilder&) = delete;
  ~CompanionUrlBuilder();

  // Returns the companion URL that will be loaded in the side panel with the
  // query parameter set to the protobuf representation of the `page_url` and
  // associated state.
  GURL BuildCompanionURL(GURL page_url);

  // Util method to check whether make searches and browsing better option is
  // enabled in the settings.
  bool IsMsbbEnabled();

 private:
  // The base URL for companion.
  GURL GetHomepageURLForCompanion();

  raw_ptr<PrefService> pref_service_;
};

}  // namespace companion

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_COMPANION_COMPANION_URL_BUILDER_H_
