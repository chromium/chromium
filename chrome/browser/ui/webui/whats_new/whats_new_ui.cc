// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"

#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "chrome/grit/whats_new_resources.h"
#include "chrome/grit/whats_new_resources_map.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

void CreateAndAddWhatsNewUIHtmlSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIWhatsNewHost);

  webui::SetupWebUIDataSource(
      source, base::make_span(kWhatsNewResources, kWhatsNewResourcesSize),
      IDR_WHATS_NEW_WHATS_NEW_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_WHATS_NEW_TITLE},
  };
  source->AddLocalizedStrings(kStrings);

  // Allow embedding of iframe from chrome.com
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      base::StringPrintf("child-src chrome://webui-test https: %s;",
                         whats_new::kChromeWhatsNewURLShort));
}

}  // namespace

// static
void WhatsNewUI::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kLastWhatsNewVersion, 0);
}

WhatsNewUI::WhatsNewUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      browser_command_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)) {
  CreateAndAddWhatsNewUIHtmlSource(profile_);
  web_ui->AddMessageHandler(std::make_unique<WhatsNewHandler>());
}

// static
base::RefCountedMemory* WhatsNewUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_NTP_FAVICON, scale_factor));
}

WEB_UI_CONTROLLER_TYPE_IMPL(WhatsNewUI)

void WhatsNewUI::BindInterface(
    mojo::PendingReceiver<browser_command::mojom::CommandHandlerFactory>
        pending_receiver) {
  if (browser_command_factory_receiver_.is_bound())
    browser_command_factory_receiver_.reset();
  browser_command_factory_receiver_.Bind(std::move(pending_receiver));
}

void WhatsNewUI::CreateBrowserCommandHandler(
    mojo::PendingReceiver<browser_command::mojom::CommandHandler>
        pending_handler) {
  std::vector<browser_command::mojom::Command> supported_commands = {
      browser_command::mojom::Command::kOpenPerformanceSettings,
      browser_command::mojom::Command::kOpenNTPAndStartCustomizeChromeTutorial,
  };
  command_handler_ = std::make_unique<BrowserCommandHandler>(
      std::move(pending_handler), profile_, supported_commands);
}

WhatsNewUI::~WhatsNewUI() = default;
