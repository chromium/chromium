// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_GCM_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_GCM_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://gcm-internals.
class GCMInternalsUI : public content::WebUIController {
 public:
  explicit GCMInternalsUI(content::WebUI* web_ui);

  GCMInternalsUI(const GCMInternalsUI&) = delete;
  GCMInternalsUI& operator=(const GCMInternalsUI&) = delete;

  ~GCMInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_GCM_INTERNALS_UI_H_
