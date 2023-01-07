// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PAGE_NOT_AVAILABLE_FOR_GUEST_PAGE_NOT_AVAILABLE_FOR_GUEST_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PAGE_NOT_AVAILABLE_FOR_GUEST_PAGE_NOT_AVAILABLE_FOR_GUEST_UI_H_

#include "content/public/browser/web_ui_controller.h"

class PageNotAvailableForGuestUI : public content::WebUIController {
 public:
  explicit PageNotAvailableForGuestUI(content::WebUI* web_ui,
                                      const std::string& host_name);

  PageNotAvailableForGuestUI(const PageNotAvailableForGuestUI&) = delete;
  PageNotAvailableForGuestUI& operator=(const PageNotAvailableForGuestUI&) =
      delete;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PAGE_NOT_AVAILABLE_FOR_GUEST_PAGE_NOT_AVAILABLE_FOR_GUEST_UI_H_
