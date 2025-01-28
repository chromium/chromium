// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_dialog_untrusted_ui.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_service_factory.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/privacy_sandbox_resources.h"
#include "chrome/grit/privacy_sandbox_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "ui/native_theme/native_theme.h"
#include "url/gurl.h"

namespace {

using enum privacy_sandbox::PrivacyPolicyDomainType;
using enum privacy_sandbox::PrivacyPolicyColorScheme;

PrivacySandboxService* GetPrivacySandboxService(content::WebUI* web_ui) {
  auto* privacy_sandbox_service =
      PrivacySandboxServiceFactory::GetForProfile(Profile::FromWebUI(web_ui));
  CHECK(privacy_sandbox_service);
  return privacy_sandbox_service;
}

}  // namespace

PrivacySandboxDialogUntrustedUIConfig::PrivacySandboxDialogUntrustedUIConfig()
    : DefaultWebUIConfig(content::kChromeUIUntrustedScheme,
                         chrome::kChromeUIPrivacySandboxDialogHost) {}

PrivacySandboxDialogUntrustedUIConfig::
    ~PrivacySandboxDialogUntrustedUIConfig() = default;

PrivacySandboxDialogUntrustedUI::PrivacySandboxDialogUntrustedUI(
    content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source =
      content::WebUIDataSource::CreateAndAdd(
          web_ui->GetWebContents()->GetBrowserContext(),
          chrome::kChromeUIUntrustedPrivacySandboxDialogURL);

  bool should_use_china_domain =
      GetPrivacySandboxService(web_ui)->ShouldUsePrivacyPolicyChinaDomain();

  std::string privacy_policy_domain = should_use_china_domain
                                          ? "https://policies.google.cn;"
                                          : "https://policies.google.com;";

  // Allows google pages to be embedded within the untrusted source.
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FrameSrc,
      "frame-src " + privacy_policy_domain);
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ObjectSrc,
      "object-src " + privacy_policy_domain);
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome-untrusted://resources 'self' 'unsafe-inline';");
  untrusted_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' chrome-untrusted://resources chrome-untrusted://theme "
      "'unsafe-inline';");

  untrusted_source->AddResourcePath(
      chrome::kChromeUIUntrustedPrivacySandboxDialogPrivacyPolicyPath,
      IDR_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PRIVACY_POLICY_HTML);

  // Dark mode support.
  ThemeService::BrowserColorScheme browser_color_scheme =
      ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui))
          ->GetBrowserColorScheme();
  bool is_dark_mode =
      (browser_color_scheme == ThemeService::BrowserColorScheme::kSystem)
          ? ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
          : browser_color_scheme == ThemeService::BrowserColorScheme::kDark;

  untrusted_source->AddString("privacyPolicyURL",
                              privacy_sandbox::GetEmbeddedPrivacyPolicyURL(
                                  should_use_china_domain ? kChina : kNonChina,
                                  is_dark_mode ? kDarkMode : kLightMode,
                                  g_browser_process->GetApplicationLocale()));

  untrusted_source->AddFrameAncestor(
      GURL(chrome::kChromeUIPrivacySandboxDialogURL));
}

PrivacySandboxDialogUntrustedUI::~PrivacySandboxDialogUntrustedUI() = default;
