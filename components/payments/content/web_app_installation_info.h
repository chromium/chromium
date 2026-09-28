// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_WEB_APP_INSTALLATION_INFO_H_
#define COMPONENTS_PAYMENTS_CONTENT_WEB_APP_INSTALLATION_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/supported_delegations.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace payments {

// Represents installation information for a web-based payment app parsed from a
// Web App Manifest.
struct WebAppInstallationInfo {
  WebAppInstallationInfo();
  ~WebAppInstallationInfo();

  std::unique_ptr<SkBitmap> icon;
  std::string name;
  std::string sw_js_url;
  std::string sw_scope;
  bool sw_use_cache = false;

  // If "prefer_related_applications" is true in web app manifest, this is the
  // list of all "related_applications.id" values where "platform" is "play".
  std::vector<std::string> preferred_app_ids;

  // List of supported delegations for this payment app.
  content::SupportedDelegations supported_delegations;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_WEB_APP_INSTALLATION_INFO_H_
