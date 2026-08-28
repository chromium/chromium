// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/location_bar_override_data.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace location_bar {

LocationBarOverrideData::LocationBarOverrideData(
    content::WebContents* web_contents,
    LocationBar* location_bar)
    : content::WebContentsUserData<LocationBarOverrideData>(*web_contents),
      location_bar_(location_bar ? location_bar->GetWeakPtr() : nullptr) {}

LocationBarOverrideData::~LocationBarOverrideData() = default;

LocationBar* GetLocationBarForWebContents(content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  if (auto* override_data =
          LocationBarOverrideData::FromWebContents(web_contents)) {
    return override_data->GetLocationBar();
  }
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    if (BrowserWindowInterface* browser = tab->GetBrowserWindowInterface()) {
      return browser->GetFeatures().location_bar();
    }
  }
  return nullptr;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LocationBarOverrideData);

}  // namespace location_bar
