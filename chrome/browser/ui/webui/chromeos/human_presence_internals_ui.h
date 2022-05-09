// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_HUMAN_PRESENCE_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_HUMAN_PRESENCE_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// The WebUI for chrome://hps-internals.
class HumanPresenceInternalsUI : public content::WebUIController {
 public:
  explicit HumanPresenceInternalsUI(content::WebUI* web_ui);

  HumanPresenceInternalsUI(const HumanPresenceInternalsUI&) = delete;
  HumanPresenceInternalsUI& operator=(const HumanPresenceInternalsUI&) = delete;

  ~HumanPresenceInternalsUI() override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_HUMAN_PRESENCE_INTERNALS_UI_H_
