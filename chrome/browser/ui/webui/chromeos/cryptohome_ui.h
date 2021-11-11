// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_UI_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_UI_H_

#include "content/public/browser/web_ui_controller.h"

namespace chromeos {

// WebUIController for chrome://cryptohome.
class CryptohomeUI : public content::WebUIController {
 public:
  explicit CryptohomeUI(content::WebUI* web_ui);

  CryptohomeUI(const CryptohomeUI&) = delete;
  CryptohomeUI& operator=(const CryptohomeUI&) = delete;

  ~CryptohomeUI() override {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_CRYPTOHOME_UI_H_
