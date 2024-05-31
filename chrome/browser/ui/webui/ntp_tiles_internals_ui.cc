// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp_tiles_internals_ui.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/ntp_tiles/chrome_most_visited_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/grit/dev_ui_components_resources.h"
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
#include "services/network/public/mojom/content_security_policy.mojom.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/new_tab_page/ntp_pref_names.h"
#include "components/prefs/pref_service.h"
#endif

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

  ChromeNTPTilesInternalsMessageHandlerClient(
      const ChromeNTPTilesInternalsMessageHandlerClient&) = delete;
  ChromeNTPTilesInternalsMessageHandlerClient& operator=(
      const ChromeNTPTilesInternalsMessageHandlerClient&) = delete;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;

  // ntp_tiles::NTPTilesInternalsMessageHandlerClient
  bool SupportsNTPTiles() override;
  std::unique_ptr<ntp_tiles::MostVisitedSites> MakeMostVisitedSites() override;
  PrefService* GetPrefs() override;
  void RegisterMessageCallback(
      std::string_view message,
      base::RepeatingCallback<void(const base::Value::List&)> callback)
      override;
  void CallJavascriptFunctionSpan(
      std::string_view name,
      base::span<const base::ValueView> values) override;

  ntp_tiles::NTPTilesInternalsMessageHandler handler_;
};

void ChromeNTPTilesInternalsMessageHandlerClient::RegisterMessages() {
  handler_.RegisterMessages(this);
}

bool ChromeNTPTilesInternalsMessageHandlerClient::SupportsNTPTiles() {
  Profile* profile = Profile::FromWebUI(web_ui());
  return !(profile->IsGuestSession() || profile->IsOffTheRecord());
}

std::unique_ptr<ntp_tiles::MostVisitedSites>
ChromeNTPTilesInternalsMessageHandlerClient::MakeMostVisitedSites() {
  auto most_visited_sites = ChromeMostVisitedSitesFactory::NewForProfile(
      Profile::FromWebUI(web_ui()));
  // Custom links only exist on Desktop.
#if !BUILDFLAG(IS_ANDROID)
  most_visited_sites->EnableCustomLinks(
      !GetPrefs()->GetBoolean(ntp_prefs::kNtpUseMostVisitedTiles));
#endif
  return most_visited_sites;
}

PrefService* ChromeNTPTilesInternalsMessageHandlerClient::GetPrefs() {
  return Profile::FromWebUI(web_ui())->GetPrefs();
}

void ChromeNTPTilesInternalsMessageHandlerClient::RegisterMessageCallback(
    std::string_view message,
    base::RepeatingCallback<void(const base::Value::List&)> callback) {
  web_ui()->RegisterMessageCallback(message, std::move(callback));
}

void ChromeNTPTilesInternalsMessageHandlerClient::CallJavascriptFunctionSpan(
    std::string_view name,
    base::span<const base::ValueView> values) {
  web_ui()->CallJavascriptFunctionUnsafe(name, values);
}

void CreateAndAddNTPTilesInternalsHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUINTPTilesInternalsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate;");

  source->AddResourcePath("ntp_tiles_internals.js", IDR_NTP_TILES_INTERNALS_JS);
  source->AddResourcePath("ntp_tiles_internals.css",
                          IDR_NTP_TILES_INTERNALS_CSS);
  source->SetDefaultResource(IDR_NTP_TILES_INTERNALS_HTML);
}

}  // namespace

NTPTilesInternalsUI::NTPTilesInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  CreateAndAddNTPTilesInternalsHTMLSource(profile);
  web_ui->AddMessageHandler(
      std::make_unique<ChromeNTPTilesInternalsMessageHandlerClient>(
          FaviconServiceFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS)));
}

NTPTilesInternalsUI::~NTPTilesInternalsUI() {}
