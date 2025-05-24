// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_SETTINGS_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_SETTINGS_H_

#include <memory>
#include <optional>

#include "components/webapps/common/web_app_id.h"

class Profile;

namespace web_app {

// This class is used by the `NavigationCapturingProcess` to get an installed
// PWA's `apps::AppId` that captures a given url. This is abstracted behind an
// interface because ChromeOS uses a separate storage / source of truth for this
// information than other platforms, and has some extra constraints &
// requirements due to needing to support ARC++ / Android apps.
//
// This class is used exclusively on the UI thread.
class NavigationCapturingSettings {
 public:
  static std::unique_ptr<NavigationCapturingSettings> Create(Profile&);
  virtual ~NavigationCapturingSettings() = default;

  // Returns the app_id for the web app that a url should be captured in.
  // Otherwise, return nullopt.
  virtual std::optional<webapps::AppId> GetCapturingWebAppForUrl(
      const GURL& url) = 0;

  // Return if auxiliary contexts should be created in the same container (app
  // or browser) that the navigation happened in. Due to breakage, this is
  // disabled by default via feature flag, but is planned to be changed to
  // always 'true'.
  //
  // In the meantime, on ChromeOS for <experiment class>, this is selectively
  // enabled for certain cases to support that project, until we can ship the
  // above change.
  virtual bool ShouldAuxiliaryContextsKeepSameContainer(
      const std::optional<webapps::AppId>& source_browser_app_id,
      const GURL& url);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_SETTINGS_H_
