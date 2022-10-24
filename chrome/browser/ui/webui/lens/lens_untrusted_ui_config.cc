// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui_config.h"

#include "chrome/browser/ui/webui/lens/lens_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"

namespace lens {

LensUntrustedUIConfig::LensUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUILensHost) {}

std::unique_ptr<content::WebUIController>
LensUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<LensUntrustedUI>(web_ui);
}

}  // namespace lens
