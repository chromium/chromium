// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONNECTION_HELP_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONNECTION_HELP_UI_H_

#include "components/security_interstitials/content/urls.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"
#include "content/public/common/url_constants.h"

namespace security_interstitials {

class ConnectionHelpUI;

class ConnectionHelpUIConfig
    : public content::DefaultWebUIConfig<ConnectionHelpUI> {
 public:
  ConnectionHelpUIConfig()
      : DefaultWebUIConfig(content::kChromeUIScheme,
                           kChromeUIConnectionHelpHost) {}
};

// The WebUI for chrome://connection-help, which provides help content to users
// with network configuration problems that prevent them from making secure
// connections.
class ConnectionHelpUI : public content::WebUIController {
 public:
  explicit ConnectionHelpUI(content::WebUI* web_ui);

  ConnectionHelpUI(const ConnectionHelpUI&) = delete;
  ConnectionHelpUI& operator=(const ConnectionHelpUI&) = delete;

  ~ConnectionHelpUI() override;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_CONNECTION_HELP_UI_H_
