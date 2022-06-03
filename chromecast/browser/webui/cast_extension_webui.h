// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_WEBUI_CAST_EXTENSION_WEBUI_H_
#define CHROMECAST_BROWSER_WEBUI_CAST_EXTENSION_WEBUI_H_

#include <string>

#include "chromecast/browser/webui/cast_webui.h"
#include "chromecast/browser/webui/mojom/webui.mojom.h"
#include "extensions/browser/extension_function_dispatcher.h"

namespace content {
class WebContents;
class WebUI;
}  // namespace content

namespace chromecast {

class CastExtensionWebUI
    : public CastWebUI,
      public extensions::ExtensionFunctionDispatcher::Delegate {
 public:
  CastExtensionWebUI(content::WebUI* webui,
                     const std::string& host,
                     mojom::WebUiClient* client);
  ~CastExtensionWebUI() override;

  // extensions::ExtensionFunctionDispatcher::Delegate implementation:
  content::WebContents* GetAssociatedWebContents() const override;
};

}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_WEBUI_CAST_EXTENSION_WEBUI_H_
