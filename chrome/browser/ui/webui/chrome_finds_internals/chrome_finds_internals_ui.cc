// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals_ui.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_agent.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_agent_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_finds_internals_resources.h"
#include "chrome/grit/chrome_finds_internals_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/webui/webui_util.h"

namespace chrome_finds_internals {

class PageHandlerImpl : public mojom::PageHandler,
                        public ChromeFindsAgent::Observer {
 public:
  PageHandlerImpl(mojo::PendingReceiver<mojom::PageHandler> receiver,
                  mojo::PendingRemote<mojom::Page> page,
                  ChromeFindsAgent* agent)
      : receiver_(this, std::move(receiver)),
        page_(std::move(page)),
        agent_(agent) {
    agent_->AddObserver(this);

    // Send existing logs.
    for (const auto& log : agent_->GetLogs()) {
      page_->LogMessageAdded(log);
    }
  }

  ~PageHandlerImpl() override { agent_->RemoveObserver(this); }

  PageHandlerImpl(const PageHandlerImpl&) = delete;
  PageHandlerImpl& operator=(const PageHandlerImpl&) = delete;

  // mojom::PageHandler:
  void Start(const std::string& prompt, int32_t history_count) override {
    agent_->Start(prompt, history_count);
  }

  void GetHistoryJson(int32_t history_count,
                      GetHistoryJsonCallback callback) override {
    agent_->GetHistoryJson(history_count, std::move(callback));
  }

  // ChromeFindsAgent::Observer:
  void OnLogMessageAdded(const std::string& message) override {
    page_->LogMessageAdded(message);
  }

 private:
  mojo::Receiver<mojom::PageHandler> receiver_;
  mojo::Remote<mojom::Page> page_;
  raw_ptr<ChromeFindsAgent> agent_;
};

bool ChromeFindsInternalsUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(features::kChromeFindsInternals);
}

ChromeFindsInternalsUI::ChromeFindsInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIChromeFindsInternalsHost);
  webui::SetupWebUIDataSource(source, kChromeFindsInternalsResources,
                              IDR_CHROME_FINDS_INTERNALS_INDEX_HTML);
}

ChromeFindsInternalsUI::~ChromeFindsInternalsUI() = default;

void ChromeFindsInternalsUI::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(receiver));
}

void ChromeFindsInternalsUI::CreatePageHandler(
    mojo::PendingRemote<mojom::Page> page,
    mojo::PendingReceiver<mojom::PageHandler> receiver) {
  Profile* profile = Profile::FromWebUI(web_ui());
  ChromeFindsAgent* agent = ChromeFindsAgentFactory::GetForProfile(profile);
  page_handler_ = std::make_unique<PageHandlerImpl>(std::move(receiver),
                                                    std::move(page), agent);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ChromeFindsInternalsUI)

}  // namespace chrome_finds_internals
