// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/grit/components_resources.h"
#include "components/history/core/browser/top_sites.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/ntp_tiles/features.h"
#include "components/ntp_tiles/icon_cacher.h"
#include "components/ntp_tiles/most_visited_sites.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler.h"
#include "components/ntp_tiles/webui/ntp_tiles_internals_message_handler_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace {

// The implementation for the chrome://ntp-tiles-internals page.
class ChromeNTPTilesInternalsMessageHandlerClient
    : public content::WebUIMessageHandler,
      public ntp_tiles::NTPTilesInternalsMessageHandlerClient {
 public:
  // |favicon_service| must not be null and must outlive this object.
  explicit ChromeNTPTilesInternalsMessageHandlerClient(
      favicon::FaviconService* favicon_service)
      : handler_(favicon_service) {}

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ntp_tiles::NTPTilesInternalsMessageHandlerClient
  bool SupportsNTPTiles() override;
  bool DoesSourceExist(ntp_tiles::TileSource source) override;
  std::unique_ptr<ntp_tiles::MostVisitedSites> MakeMostVisitedSites() override;
  PrefService* GetPrefs() override;
  void RegisterMessageCallback(
      const std::string& message,
      const base::RepeatingCallback<void(const base::ListValue*)>& callback)
      override;
  void CallJavascriptFunctionVector(
      const std::string& name,
      const std::vector<const base::Value*>& values) override;

  ntp_tiles::NTPTilesInternalsMessageHandler handler_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNTPTilesInternalsMessageHandlerClient);
};

void ChromeNTPTilesInternalsMessageHandlerClient::RegisterMessages() {
  handler_.RegisterMessages(this);
}

bool ChromeNTPTilesInternalsMessageHandlerClient::SupportsNTPTiles() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return !(profile->IsGuestSession() || profile->IsOffTheRecord());
}

bool ChromeNTPTilesInternalsMessageHandlerClient::DoesSourceExist(
    ntp_tiles::TileSource source) {
  switch (source) {
    case ntp_tiles::TileSource::TOP_SITES:
    case ntp_tiles::TileSource::SUGGESTIONS_SERVICE:
    case ntp_tiles::TileSource::WHITELIST:
    case ntp_tiles::TileSource::HOMEPAGE:
      return true;
    case ntp_tiles::TileSource::POPULAR_BAKED_IN:
    case ntp_tiles::TileSource::POPULAR:
    case ntp_tiles::TileSource::EXPLORE:
#if defined(OS_ANDROID)
      return true;
#else
      return false;
#endif
    case ntp_tiles::TileSource::CUSTOM_LINKS:
#if !defined(OS_ANDROID)
      return true;
#else
      return false;
#endif
  }
  NOTREACHED();
  return false;
}

std::unique_ptr<ntp_tiles::MostVisitedSites>
ChromeNTPTilesInternalsMessageHandlerClient::MakeMostVisitedSites() {
  return ChromeMostVisitedSitesFactory::NewForProfile(
      Profile::FromWebUI(web_ui()));
}

PrefService* ChromeNTPTilesInternalsMessageHandlerClient::GetPrefs() {
  return Profile::FromWebUI(web_ui())->GetPrefs();
}

void ChromeNTPTilesInternalsMessageHandlerClient::RegisterMessageCallback(
    const std::string& message,
    const base::RepeatingCallback<void(const base::ListValue*)>& callback) {
  web_ui()->RegisterMessageCallback(message, callback);
}

void ChromeNTPTilesInternalsMessageHandlerClient::CallJavascriptFunctionVector(
    const std::string& name,
    const std::vector<const base::Value*>& values) {
  web_ui()->CallJavascriptFunctionUnsafe(name, values);
}

content::WebUIDataSource* CreateNTPTilesInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUINTPTilesInternalsHost);
  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  source->AddResourcePath("ntp_tiles_internals.js", IDR_NTP_TILES_INTERNALS_JS);
  source->AddResourcePath("ntp_tiles_internals.css",
                          IDR_NTP_TILES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_NTP_TILES_INTERNALS_HTML);
  return source;
}

}  // namespace

NTPTilesInternalsUI::NTPTilesInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateNTPTilesInternalsHTMLSource());
  web_ui->AddMessageHandler(
      std::make_unique<ChromeNTPTilesInternalsMessageHandlerClient>(
          FaviconServiceFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS)));
}

NTPTilesInternalsUI::~NTPTilesInternalsUI() {}
