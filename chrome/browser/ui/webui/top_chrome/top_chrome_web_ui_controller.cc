// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/top_chrome/top_chrome_web_ui_controller.h"

#include "content/public/browser/web_ui.h"

TopChromeWebUIController::TopChromeWebUIController(content::WebUI* contents,
                                                     bool enable_chrome_send)
    : MojoWebUIController(contents, enable_chrome_send) {}

TopChromeWebUIController::~TopChromeWebUIController() = default;
