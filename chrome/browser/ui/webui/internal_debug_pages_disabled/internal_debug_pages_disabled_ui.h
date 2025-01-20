// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNAL_DEBUG_PAGES_DISABLED_INTERNAL_DEBUG_PAGES_DISABLED_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNAL_DEBUG_PAGES_DISABLED_INTERNAL_DEBUG_PAGES_DISABLED_UI_H_

#include "content/public/browser/web_ui_controller.h"

class InternalDebugPagesDisabledUI : public content::WebUIController {
 public:
  explicit InternalDebugPagesDisabledUI(content::WebUI* web_ui,
                                        const std::string& host_name);

  InternalDebugPagesDisabledUI(const InternalDebugPagesDisabledUI&) = delete;
  InternalDebugPagesDisabledUI& operator=(const InternalDebugPagesDisabledUI&) =
      delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNAL_DEBUG_PAGES_DISABLED_INTERNAL_DEBUG_PAGES_DISABLED_UI_H_
