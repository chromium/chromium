// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_UI_H_

#include <memory>

#include "chrome/common/webui_url_constants.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "content/public/browser/internal_webui_config.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
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
class AutofillMlInternalsUI
    : public ui::MojoWebUIController,
      public autofill_ml_internals::mojom::PageHandlerFactory {
 public:
  explicit AutofillMlInternalsUI(content::WebUI* web_ui);
  AutofillMlInternalsUI(const AutofillMlInternalsUI&) = delete;
  AutofillMlInternalsUI& operator=(const AutofillMlInternalsUI&) = delete;
  ~AutofillMlInternalsUI() override;

  void BindInterface(
      mojo::PendingReceiver<autofill_ml_internals::mojom::PageHandlerFactory>
          receiver);

 private:
  // autofill_ml_internals::mojom::PageHandlerFactory:
  void CreatePageHandler(
      mojo::PendingRemote<autofill_ml_internals::mojom::Page> page,
      mojo::PendingReceiver<autofill_ml_internals::mojom::PageHandler> receiver)
      override;

  WEB_UI_CONTROLLER_TYPE_DECL();

  std::unique_ptr<autofill_ml_internals::mojom::PageHandler> page_handler_;
  mojo::Receiver<autofill_ml_internals::mojom::PageHandlerFactory>
      factory_receiver_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_UI_H_
