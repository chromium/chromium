// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/webui/chromeos/login/base_webui_handler.h"

namespace chromeos {

// Base class for Discover modules WebUI message handler.
class DiscoverHandler : public BaseWebUIHandler {
 public:
  explicit DiscoverHandler(JSCallsContainer* js_calls_container);
  ~DiscoverHandler() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(DiscoverHandler);
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_DISCOVER_DISCOVER_HANDLER_H_
