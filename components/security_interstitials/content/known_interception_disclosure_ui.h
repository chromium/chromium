// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_KNOWN_INTERCEPTION_DISCLOSURE_UI_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_KNOWN_INTERCEPTION_DISCLOSURE_UI_H_

#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/webui_config.h"

namespace content {
class WebUI;
}

namespace security_interstitials {

class KnownInterceptionDisclosureUI;

class KnownInterceptionDisclosureUIConfig
    : public content::DefaultWebUIConfig<KnownInterceptionDisclosureUI> {
 public:
  KnownInterceptionDisclosureUIConfig();
};

// The WebUI for chrome://connection-monitoring-detected, which provides details
// to users when Chrome has detected known network interception.
class KnownInterceptionDisclosureUI : public content::WebUIController {
 public:
  explicit KnownInterceptionDisclosureUI(content::WebUI* web_ui);
  ~KnownInterceptionDisclosureUI() override;
  KnownInterceptionDisclosureUI(const KnownInterceptionDisclosureUI&) = delete;
  KnownInterceptionDisclosureUI& operator=(
      const KnownInterceptionDisclosureUI&) = delete;
};

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CONTENT_KNOWN_INTERCEPTION_DISCLOSURE_UI_H_
