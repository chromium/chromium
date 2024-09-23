// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/webui_util.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/resources/grit/webui_resources.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/startup/browser_params_proxy.h"
#endif

namespace webui {

void SetJSModuleDefaults(content::WebUIDataSource* source) {
  std::string scheme =
      base::StartsWith(source->GetSource(), content::kChromeUIUntrustedScheme)
          ? content::kChromeUIUntrustedScheme
          : content::kChromeUIScheme;
  // Set the default src to 'self' only. Generally necessary overrides are set
  // below. Additional overrides should generally be added for individual
  // data sources only.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "default-src 'self';");
  // Allow connecting to chrome://resources for trusted UIs and images from
  // chrome://image, chrome://theme, chrome://favicon2, chrome://resources and
  // data URLs.
  // TODO: Both of these are needed by only a smaller subset of UIs. Should
  // the overrides be moved to those UIs only?
  if (scheme == content::kChromeUIScheme) {
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ConnectSrc,
        "connect-src chrome://resources chrome://theme 'self';");
    source->OverrideContentSecurityPolicy(
        network::mojom::CSPDirectiveName::ImgSrc,
        "img-src chrome://resources chrome://theme chrome://image "
        "chrome://favicon2 chrome://app-icon chrome://extension-icon "
        "chrome://fileicon "
#if BUILDFLAG(IS_CHROMEOS_ASH)
        "chrome://chromeos-asset chrome://userimage "
#endif
        "blob: data: 'self';");
  }

  // webui-test is required for tests. Scripts from //resources are allowed
  // for all UIs.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      base::StringPrintf("script-src %s://resources %s://webui-test 'self';",
                         scheme.c_str(), scheme.c_str()));
  // Allow loading fonts from //resources.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::FontSrc,
      base::StringPrintf("font-src %s://resources 'self';", scheme.c_str()));
  // unsafe-inline is required for Polymer. Allow styles to be imported from
  // //resources and //theme.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      base::StringPrintf(
          "style-src %s://resources %s://theme 'self' 'unsafe-inline';",
          scheme.c_str(), scheme.c_str()));

  source->UseStringsJs();
  source->EnableReplaceI18nInJS();
  source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER_JS);
  source->AddResourcePath("test_loader_util.js",
                          IDR_WEBUI_JS_TEST_LOADER_UTIL_JS);
  source->AddResourcePath("test_loader.html", IDR_WEBUI_TEST_LOADER_HTML);
}

void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const ResourcePath> resources,
                          int default_resource) {
  SetJSModuleDefaults(source);
  EnableTrustedTypesCSP(source);
  source->AddResourcePaths(resources);
  source->AddResourcePath("", default_resource);
}

// There is another method, ash::EnableTrustedTypesCSP, used by ash-only WebUIs.
// When adding a new policy here, consider whether to add it to that method as
// well, as these methods should remain mostly the same.
void EnableTrustedTypesCSP(content::WebUIDataSource* source) {
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::RequireTrustedTypesFor,
      "require-trusted-types-for 'script';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types parse-html-subset sanitize-inner-html static-types "
      // Add TrustedTypes policies for cr-lottie.
      "lottie-worker-script-loader "
      // Add TrustedTypes policies used during tests.
      "webui-test-script webui-test-html "
      // Add TrustedTypes policy for creating the PDF plugin.
      "print-preview-plugin-loader "
      // Add TrustedTypes policies necessary for using Polymer.
      "polymer-html-literal polymer-template-event-attribute-policy "
      // Add TrustedTypes policies necessary for using Desktop's Lit bundle.
      "lit-html-desktop;");
}

void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id) {
  std::u16string str = l10n_util::GetStringUTF16(id);
  std::erase(str, '&');
  source->AddString(message, str);
}

}  // namespace webui
