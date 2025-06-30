// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_UI_H_

#include "chrome/common/webui_url_constants.h"
#include "content/public/browser/internal_webui_config.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/webui/mojo_web_ui_controller.h"

class AutofillMlInternalsUI;

// The WebUIConfig for chrome://autofill-ml-internals
class AutofillMlInternalsUIConfig
    : public content::DefaultInternalWebUIConfig<AutofillMlInternalsUI> {
 public:
  AutofillMlInternalsUIConfig()
      : content::DefaultInternalWebUIConfig<AutofillMlInternalsUI>(
            chrome::kChromeUIAutofillMlInternalsHost) {}
};

// The WebUIController for chrome://autofill-ml-internals
class AutofillMlInternalsUI : public content::WebUIController {
 public:
  explicit AutofillMlInternalsUI(content::WebUI* web_ui);
  AutofillMlInternalsUI(const AutofillMlInternalsUI&) = delete;
  AutofillMlInternalsUI& operator=(const AutofillMlInternalsUI&) = delete;
  ~AutofillMlInternalsUI() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_UI_H_
