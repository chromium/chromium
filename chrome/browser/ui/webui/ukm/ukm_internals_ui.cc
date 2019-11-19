// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ukm/ukm_internals_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/ukm/debug/ukm_debug_data_extractor.h"
#include "components/ukm/ukm_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

content::WebUIDataSource* CreateUkmHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIUkmHost);

  source->AddResourcePath("ukm_internals.js", IDR_UKM_INTERNALS_JS);
  source->AddResourcePath("ukm_internals.css", IDR_UKM_INTERNALS_CSS);
  source->SetDefaultResource(IDR_UKM_INTERNALS_HTML);
  return source;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class UkmMessageHandler : public content::WebUIMessageHandler {
 public:
  explicit UkmMessageHandler(const ukm::UkmService* ukm_service);
  ~UkmMessageHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

 private:
  void HandleRequestUkmData(const base::ListValue* args);

  const ukm::UkmService* ukm_service_;

  DISALLOW_COPY_AND_ASSIGN(UkmMessageHandler);
};

UkmMessageHandler::UkmMessageHandler(const ukm::UkmService* ukm_service)
    : ukm_service_(ukm_service) {}

UkmMessageHandler::~UkmMessageHandler() {}

void UkmMessageHandler::HandleRequestUkmData(const base::ListValue* args) {
  AllowJavascript();

  // Identifies the callback, used for when resolving.
  std::string callback_id;
  args->GetString(0, &callback_id);

  base::Value ukm_debug_data =
      ukm::debug::UkmDebugDataExtractor::GetStructuredData(ukm_service_);

  ResolveJavascriptCallback(base::Value(callback_id),
                            std::move(ukm_debug_data));
}

void UkmMessageHandler::RegisterMessages() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // We can use base::Unretained() here, as both the callback and this class are
  // owned by UkmInternalsUI.
  web_ui()->RegisterMessageCallback(
      "requestUkmData",
      base::BindRepeating(&UkmMessageHandler::HandleRequestUkmData,
                          base::Unretained(this)));
}

}  // namespace

// Changes to this class should be in sync with its iOS equivalent
// ios/chrome/browser/ui/webui/ukm_internals_ui.cc
UkmInternalsUI::UkmInternalsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  ukm::UkmService* ukm_service =
      g_browser_process->GetMetricsServicesManager()->GetUkmService();
  web_ui->AddMessageHandler(std::make_unique<UkmMessageHandler>(ukm_service));

  // Set up the chrome://ukm/ source.
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource::Add(browser_context, CreateUkmHTMLSource());
}
