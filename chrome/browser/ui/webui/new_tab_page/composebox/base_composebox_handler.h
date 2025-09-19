// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_BASE_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_BASE_COMPOSEBOX_HANDLER_H_

#include <map>
#include <string>

#include "ui/base/window_open_disposition_utils.h"

namespace composebox {

// Base class for ComposeboxHandler and LensComposeboxHandler.
class BaseComposeboxHandler {
 public:
  virtual ~BaseComposeboxHandler() = default;

  // Submits the query with the given text and disposition.
  virtual void SubmitQuery(
      const std::string& query_text,
      WindowOpenDisposition disposition,
      std::map<std::string, std::string> additional_params) = 0;
};

}  // namespace composebox

#endif  // CHROME_BROWSER_UI_WEBUI_NEW_TAB_PAGE_COMPOSEBOX_BASE_COMPOSEBOX_HANDLER_H_
