// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_UI_H_

#include "content/public/browser/web_ui_controller.h"

class MemoryInternalsUI : public content::WebUIController {
 public:
  explicit MemoryInternalsUI(content::WebUI* web_ui);

  MemoryInternalsUI(const MemoryInternalsUI&) = delete;
  MemoryInternalsUI& operator=(const MemoryInternalsUI&) = delete;

  ~MemoryInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_UI_H_
