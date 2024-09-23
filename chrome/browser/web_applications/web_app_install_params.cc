// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_install_params.h"

#include "chrome/browser/web_applications/web_app_install_info.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

WebAppInstallParams::WebAppInstallParams() = default;

WebAppInstallParams::~WebAppInstallParams() = default;

WebAppInstallParams::WebAppInstallParams(const WebAppInstallParams&) = default;

std::ostream& operator<<(std::ostream& os, FallbackBehavior state) {
  switch (state) {
    case FallbackBehavior::kCraftedManifestOnly:
      return os << "kCraftedManifestOnly";
    case FallbackBehavior::kUseFallbackInfoWhenNotInstallable:
      return os << "kUseFallbackInfoWhenNotInstallable";
    case FallbackBehavior::kAllowFallbackDataAlways:
      return os << "kAllowFallbackDataAlways";
  }
}

}  // namespace web_app
