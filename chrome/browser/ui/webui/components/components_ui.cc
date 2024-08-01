// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/components/components_ui.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/components/components_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/components_resources.h"
#include "chrome/grit/components_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

void CreateAndAddComponentsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIComponentsHost);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self' 'unsafe-eval';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types jstemplate parse-html-subset;");

  static constexpr webui::LocalizedString kStrings[] = {
    {"componentsTitle", IDS_COMPONENTS_TITLE},
    {"componentsNoneInstalled", IDS_COMPONENTS_NONE_INSTALLED},
    {"componentVersion", IDS_COMPONENTS_VERSION},
    {"checkUpdate", IDS_COMPONENTS_CHECK_FOR_UPDATE},
    {"noComponents", IDS_COMPONENTS_NO_COMPONENTS},
    {"statusLabel", IDS_COMPONENTS_STATUS_LABEL},
    {"checkingLabel", IDS_COMPONENTS_CHECKING_LABEL},
#if BUILDFLAG(IS_CHROMEOS)
    {"os-components-text1", IDS_COMPONENTS_OS_TEXT1_LABEL},
    {"os-components-text2", IDS_COMPONENTS_OS_TEXT2_LABEL},
    {"os-components-link", IDS_COMPONENTS_OS_LINK},
#endif
  };
  source->AddLocalizedStrings(kStrings);

  source->AddBoolean(
      "isGuest",
#if BUILDFLAG(IS_CHROMEOS_ASH)
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsManagedGuestSession()
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
                      chromeos::BrowserParamsProxy::Get()->SessionType() ==
                              crosapi::mojom::SessionType::kPublicSession ||
                          profile->IsGuestSession()
#else
      profile->IsOffTheRecord()
#endif
  );
  source->UseStringsJs();
  source->AddResourcePaths(
      base::make_span(kComponentsResources, kComponentsResourcesSize));
  source->SetDefaultResource(IDR_COMPONENTS_COMPONENTS_HTML);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ComponentsUI
//
///////////////////////////////////////////////////////////////////////////////

ComponentsUI::ComponentsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ComponentsHandler>(
      g_browser_process->component_updater()));

  // Set up the chrome://components/ source.
  CreateAndAddComponentsUIHTMLSource(Profile::FromWebUI(web_ui));
}

ComponentsUI::~ComponentsUI() {}

// static
base::RefCountedMemory* ComponentsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_PLUGINS_FAVICON, scale_factor);
}
