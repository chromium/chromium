// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_SETTINGS_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_SETTINGS_IMPL_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/web_applications/navigation_capturing_settings.h"

class Profile;

namespace web_app {

// This is created in the `NavigationCapturingSettings::Create` method, called
// in `NavigationCapturingProcess`.
//
// Windows, Mac, and Linux queries through the WebAppRegistrar to store the
// preferred application to capturing navigations given a url.
class NavigationCapturingSettingsImpl : public NavigationCapturingSettings {
 public:
  explicit NavigationCapturingSettingsImpl(Profile& profile);
  ~NavigationCapturingSettingsImpl() override;
  NavigationCapturingSettingsImpl(const NavigationCapturingSettingsImpl&) =
      delete;
  NavigationCapturingSettingsImpl& operator=(
      const NavigationCapturingSettingsImpl&) = delete;

  std::optional<webapps::AppId> GetCapturingWebAppForUrl(
      const GURL& url) override;

 private:
  raw_ref<Profile> profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_NAVIGATION_CAPTURING_SETTINGS_IMPL_H_
