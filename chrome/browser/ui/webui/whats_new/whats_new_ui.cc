// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"

#include "base/feature_list.h"
#include "base/version.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/global_features.h"
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
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace {

void CreateAndAddWhatsNewUIHtmlSource(Profile* profile, bool enable_staging) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIWhatsNewHost);

  webui::SetupWebUIDataSource(
      source, base::span<const webui::ResourcePath>(kWhatsNewResources),
      IDR_WHATS_NEW_WHATS_NEW_HTML);

  static constexpr webui::LocalizedString kStrings[] = {
      {"title", IDS_WHATS_NEW_TITLE},
  };
  source->AddLocalizedStrings(kStrings);
  source->AddBoolean("isWhatsNewV2", user_education::features::IsWhatsNewV2());
  source->AddBoolean("isStaging", enable_staging);

  // Allow embedding of iframe from chrome.com
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      enable_staging
          ? "child-src chrome://webui-test https://www.google.com/ "
            "https://chrome-staging.corp.google.com/;"
          : "child-src chrome://webui-test https://www.google.com/;");
}

}  // namespace

WhatsNewUIConfig::WhatsNewUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWhatsNewHost) {}

bool WhatsNewUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return whats_new::IsEnabled();
}

// static
void WhatsNewUI::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kWhatsNewEditionUsed);
  registry->RegisterListPref(prefs::kWhatsNewFirstEnabledOrder);
}

WhatsNewUI::WhatsNewUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true),
      content::WebContentsObserver(web_ui->GetWebContents()),
      page_factory_receiver_(this),
      browser_command_factory_receiver_(this),
      profile_(Profile::FromWebUI(web_ui)) {
  GURL url = web_ui->GetWebContents()->GetVisibleURL();
  bool enable_staging = url.query_piece().compare("staging=true") == 0;
  CreateAndAddWhatsNewUIHtmlSource(profile_, enable_staging);
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
    mojo::PendingReceiver<whats_new::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void WhatsNewUI::CreatePageHandler(
    mojo::PendingRemote<whats_new::mojom::Page> page,
    mojo::PendingReceiver<whats_new::mojom::PageHandler> receiver) {
  DCHECK(page);
  page_handler_ = std::make_unique<WhatsNewHandler>(
      std::move(receiver), std::move(page), profile_,
      web_ui()->GetWebContents(), navigation_start_time_);
}

void WhatsNewUI::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->GetURL() == GURL(chrome::kChromeUIWhatsNewURL)) {
    navigation_start_time_ = base::Time::Now();
  }
}

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
  std::vector<browser_command::mojom::Command> supported_commands = {};

  if (user_education::features::IsWhatsNewV2()) {
    auto* registry = g_browser_process->GetFeatures()->whats_new_registry();
    CHECK(registry);
    supported_commands = registry->GetActiveCommands();
  } else {
    // TODO(crbug.com/342172972): Remove legacy browser command format.
    // Modules launching during the V2 experiment need to also be
    // enabled in this list for V1.
    supported_commands.insert(
        supported_commands.end(),
        {
            browser_command::mojom::Command::kOpenPaymentsSettings,
            browser_command::mojom::Command::KOpenHistorySearchSettings,
        });
  }

  // Legacy list. Do not add browser commands here. Browser commands
  // should instead be added through the WhatsNewRegistry.
  supported_commands.insert(
      supported_commands.end(),
      {
          browser_command::mojom::Command::kStartSavedTabGroupTutorial,
          browser_command::mojom::Command::kOpenAISettings,
          browser_command::mojom::Command::kOpenSafetyCheckFromWhatsNew,
      });
  command_handler_ = std::make_unique<BrowserCommandHandler>(
      std::move(pending_handler), profile_, supported_commands);
}

WhatsNewUI::~WhatsNewUI() = default;
