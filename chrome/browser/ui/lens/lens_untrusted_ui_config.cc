// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_untrusted_ui_config.h"

#include "chrome/browser/ui/lens/lens_untrusted_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "content/public/common/url_constants.h"

namespace lens {

LensUntrustedUIConfig::LensUntrustedUIConfig()
    : DefaultTopChromeWebUIConfig(content::kChromeUIUntrustedScheme,
                                  chrome::kChromeUILensHost) {}

}  // namespace lens
