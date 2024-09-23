// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

class ProfileInternalsUI;

class ProfileInternalsUIConfig
    : public content::DefaultWebUIConfig<ProfileInternalsUI> {
 public:
  ProfileInternalsUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           chrome::kChromeUIProfileInternalsHost) {}
};

// Controller for chrome://profile-internals page.
class ProfileInternalsUI : public content::WebUIController {
 public:
  explicit ProfileInternalsUI(content::WebUI* web_ui);

  ProfileInternalsUI(const ProfileInternalsUI&) = delete;
  ProfileInternalsUI& operator=(const ProfileInternalsUI&) = delete;

  ~ProfileInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PROFILE_INTERNALS_PROFILE_INTERNALS_UI_H_
