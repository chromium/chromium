// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/web_ui_controller.h"

// The implementation for the chrome://sync-internals page.
class SyncInternalsUI : public content::WebUIController {
 public:
  explicit SyncInternalsUI(content::WebUI* web_ui);
  ~SyncInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_INTERNALS_SYNC_INTERNALS_UI_H_
