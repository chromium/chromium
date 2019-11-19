// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_H_

#include "base/macros.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"

class AutofillInternalsUI : public content::WebUIController {
 public:
  explicit AutofillInternalsUI(content::WebUI* web_ui);
  ~AutofillInternalsUI() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_AND_PASSWORD_MANAGER_INTERNALS_AUTOFILL_INTERNALS_UI_H_
