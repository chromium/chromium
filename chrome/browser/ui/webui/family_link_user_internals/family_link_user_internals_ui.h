// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://family-link-user-internals page.
class FamilyLinkUserInternalsUI : public content::WebUIController {
 public:
  explicit FamilyLinkUserInternalsUI(content::WebUI* web_ui);

  FamilyLinkUserInternalsUI(const FamilyLinkUserInternalsUI&) = delete;
  FamilyLinkUserInternalsUI& operator=(const FamilyLinkUserInternalsUI&) =
      delete;

  ~FamilyLinkUserInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_FAMILY_LINK_USER_INTERNALS_FAMILY_LINK_USER_INTERNALS_UI_H_
