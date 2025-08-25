// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_ml_internals/autofill_ml_internals_ui.h"

#include "chrome/browser/autofill/ml_log_router_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/autofill_ml_internals/autofill_ml_internals_page_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/autofill_ml_internals_resources.h"
#include "chrome/grit/autofill_ml_internals_resources_map.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

AutofillMlInternalsUI::AutofillMlInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  // Set up the chrome://autofill-ml-internals source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIAutofillMlInternalsHost);

  // Add required resources.
  webui::SetupWebUIDataSource(
      source, kAutofillMlInternalsResources,
      IDR_AUTOFILL_ML_INTERNALS_AUTOFILL_ML_INTERNALS_HTML);
}

void AutofillMlInternalsUI::BindInterface(
    mojo::PendingReceiver<autofill_ml_internals::mojom::PageHandler> receiver) {
  page_handler_ = std::make_unique<AutofillMlInternalsPageHandlerImpl>(
      std::move(receiver), autofill::MlLogRouterFactory::GetForProfile(
                               Profile::FromWebUI(web_ui())));
}

WEB_UI_CONTROLLER_TYPE_IMPL(AutofillMlInternalsUI)

AutofillMlInternalsUI::~AutofillMlInternalsUI() = default;
