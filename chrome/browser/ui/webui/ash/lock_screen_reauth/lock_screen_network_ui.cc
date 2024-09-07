// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_ui.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/network_config_service.h"
#include "ash/webui/common/trusted_types_util.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/lock_screen_reauth_resources.h"
#include "chrome/grit/lock_screen_reauth_resources_map.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/chromeos/strings/network/network_element_localized_strings_provider.h"

namespace ash {

bool LockScreenNetworkUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return ash::ProfileHelper::IsLockScreenProfile(
      Profile::FromBrowserContext(browser_context));
}

// static
base::Value::Dict LockScreenNetworkUI::GetLocalizedStrings() {
  base::Value::Dict localized_strings;
  localized_strings.Set(
      "titleText", l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_NETWORK_TITLE));
  localized_strings.Set(
      "lockScreenNetworkTitle",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_NETWORK_TITLE));
  localized_strings.Set(
      "lockScreenNetworkSubtitle",
      l10n_util::GetStringFUTF16(IDS_LOCK_SCREEN_NETWORK_SUBTITLE,
                                 ui::GetChromeOSDeviceName()));
  localized_strings.Set(
      "lockScreenCancelButton",
      l10n_util::GetStringUTF16(IDS_LOCK_SCREEN_CANCEL_BUTTON));
  return localized_strings;
}

LockScreenNetworkUI::LockScreenNetworkUI(content::WebUI* web_ui)
    : ui::MojoWebDialogUI(web_ui) {
  auto main_handler = std::make_unique<NetworkConfigMessageHandler>();
  main_handler_ = main_handler.get();
  web_ui->AddMessageHandler(std::move(main_handler));

  base::Value::Dict localized_strings = GetLocalizedStrings();

  content::WebUIDataSource* html = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUILockScreenNetworkHost);
  ash::EnableTrustedTypesCSP(html);

  html->AddLocalizedStrings(localized_strings);

  ui::network_element::AddLocalizedStrings(html);
  ui::network_element::AddOncLocalizedStrings(html);
  html->UseStringsJs();

  html->AddResourcePaths(base::make_span(kLockScreenReauthResources,
                                         kLockScreenReauthResourcesSize));
  html->SetDefaultResource(IDR_LOCK_SCREEN_REAUTH_LOCK_SCREEN_NETWORK_HTML);
}

LockScreenNetworkUI::~LockScreenNetworkUI() = default;

void LockScreenNetworkUI::BindInterface(
    mojo::PendingReceiver<chromeos::network_config::mojom::CrosNetworkConfig>
        receiver) {
  GetNetworkConfigService(std::move(receiver));
}

WEB_UI_CONTROLLER_TYPE_IMPL(LockScreenNetworkUI)

}  // namespace ash
