// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SANDBOX_SANDBOX_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SANDBOX_SANDBOX_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// This WebUI page displays the status of the renderer sandbox on Linux and
// Android. The two OSes share the same basic page, but the data reported are
// obtained from different places:
//    - On Linux, this object in the browser queries the renderer ZygoteHost
//      to get the sandbox status of the renderers. The data are then specified
//      as loadTimeData on the WebUI Page.
//    - On Android, this object sends an IPC message to the
//      SandboxStatusExtension in the renderer, which installs a JavaScript
//      function on the web page to return the current sandbox status.
class SandboxInternalsUI : public content::WebUIController {
 public:
  explicit SandboxInternalsUI(content::WebUI* web_ui);
  ~SandboxInternalsUI() override;

  void RenderFrameCreated(content::RenderFrameHost* render_frame_host) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SANDBOX_SANDBOX_INTERNALS_UI_H_
