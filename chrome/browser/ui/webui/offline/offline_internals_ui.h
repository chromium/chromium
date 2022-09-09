// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The WebUI for chrome://offline-internals.
class OfflineInternalsUI : public content::WebUIController {
 public:
  explicit OfflineInternalsUI(content::WebUI* web_ui);

  OfflineInternalsUI(const OfflineInternalsUI&) = delete;
  OfflineInternalsUI& operator=(const OfflineInternalsUI&) = delete;

  ~OfflineInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OFFLINE_OFFLINE_INTERNALS_UI_H_
