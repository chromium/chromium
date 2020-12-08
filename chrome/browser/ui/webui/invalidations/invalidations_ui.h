// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://invalidations page.
class InvalidationsUI : public content::WebUIController {
 public:
  explicit InvalidationsUI(content::WebUI* web_ui);
  ~InvalidationsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(InvalidationsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INVALIDATIONS_INVALIDATIONS_UI_H_
