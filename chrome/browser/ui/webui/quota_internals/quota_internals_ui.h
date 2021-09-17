// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

class QuotaInternalsUI : public content::WebUIController {
 public:
  explicit QuotaInternalsUI(content::WebUI* web_ui);

  QuotaInternalsUI(const QuotaInternalsUI&) = delete;
  QuotaInternalsUI& operator=(const QuotaInternalsUI&) = delete;

  ~QuotaInternalsUI() override {}
};

#endif  // CHROME_BROWSER_UI_WEBUI_QUOTA_INTERNALS_QUOTA_INTERNALS_UI_H_
