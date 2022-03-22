// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_HPS_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_HPS_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// The WebUI for chrome://hps-internals.
class HpsInternalsUI : public content::WebUIController {
 public:
  explicit HpsInternalsUI(content::WebUI* web_ui);

  HpsInternalsUI(const HpsInternalsUI&) = delete;
  HpsInternalsUI& operator=(const HpsInternalsUI&) = delete;

  ~HpsInternalsUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_HPS_INTERNALS_UI_H_
