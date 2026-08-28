// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_OVERRIDE_DATA_H_
#define CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_OVERRIDE_DATA_H_

#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_user_data.h"

class LocationBar;

namespace location_bar {

// A `WebContentsUserData` attached to a `content::WebContents` to override its
// associated `LocationBar`.
//
// By default, features that interact with the location bar (such as permission
// prompts, page action chips, and quiet indicators) retrieve the `LocationBar`
// from the hosting `BrowserWindow`. For secondary UI surfaces (such as the
// Contextual Tasks side panel), attaching `LocationBarOverrideData` allows
// associating a custom `LocationBar` instance with the `WebContents` so that
// these UI features interact with the secondary surface instead of the main
// browser omnibox.
class LocationBarOverrideData
    : public content::WebContentsUserData<LocationBarOverrideData> {
 public:
  ~LocationBarOverrideData() override;

  LocationBarOverrideData(const LocationBarOverrideData&) = delete;
  LocationBarOverrideData& operator=(const LocationBarOverrideData&) = delete;

  LocationBar* GetLocationBar() const { return location_bar_.get(); }

 private:
  friend class content::WebContentsUserData<LocationBarOverrideData>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  LocationBarOverrideData(content::WebContents* web_contents,
                          LocationBar* location_bar);

  base::WeakPtr<LocationBar> location_bar_;
};

// Returns the LocationBar associated with the given `web_contents`.
// If `LocationBarOverrideData` is set on `web_contents`, returns the overridden
// LocationBar. Otherwise, falls back to the LocationBar of the hosting
// `BrowserWindow` for the tab. Returns `nullptr` if `web_contents` is null,
// or if no tab/browser or LocationBar is found.
LocationBar* GetLocationBarForWebContents(content::WebContents* web_contents);

}  // namespace location_bar

#endif  // CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_OVERRIDE_DATA_H_
