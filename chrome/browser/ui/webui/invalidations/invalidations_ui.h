// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_UI_H_

#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://invalidations page.
class InvalidationsUI : public content::WebUIController {
 public:
  explicit InvalidationsUI(content::WebUI* web_ui);

  InvalidationsUI(const InvalidationsUI&) = delete;
  InvalidationsUI& operator=(const InvalidationsUI&) = delete;

  ~InvalidationsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_UI_H_
