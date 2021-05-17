// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/help_app_ui/help_app_untrusted_ui.h"

#include "base/strings/string_piece.h"
#include "chromeos/components/help_app_ui/help_app_ui_delegate.h"
#include "chromeos/components/help_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_bundle_resources_map.h"
#include "chromeos/grit/chromeos_help_app_kids_magazine_bundle_resources.h"
#include "chromeos/grit/chromeos_help_app_kids_magazine_bundle_resources_map.h"
#include "chromeos/grit/chromeos_help_app_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/resources/grit/webui_generated_resources.h"
#include "ui/resources/grit/webui_resources.h"

namespace {
// Function to remove a prefix from an input string. Does nothing if the string
// does not begin with the prefix.
base::StringPiece StripPrefix(base::StringPiece input,
                              base::StringPiece prefix) {
  if (input.find(prefix) == 0) {
    return input.substr(prefix.size());
  }
  return input;
}
}  // namespace

namespace chromeos {

// static
content::WebUIDataSource* CreateHelpAppUntrustedDataSource(
    HelpAppUIDelegate* delegate) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIHelpAppUntrustedURL);
  // app.html is the default resource because it has routing logic to handle all
  // the other paths.
  source->SetDefaultResource(IDR_HELP_APP_APP_HTML);
  source->AddResourcePath("app_bin.js", IDR_HELP_APP_APP_BIN_JS);
  source->AddResourcePath("load_time_data.js", IDR_WEBUI_JS_LOAD_TIME_DATA_JS);
  source->AddResourcePath("help_app_app_scripts.js",
                          IDR_HELP_APP_APP_SCRIPTS_JS);
  source->DisableTrustedTypesCSP();

  // Add all resources from chromeos_help_app_bundle.pak.
  source->AddResourcePaths(base::make_span(
      kChromeosHelpAppBundleResources, kChromeosHelpAppBundleResourcesSize));

  // Add device and feature flags.
  delegate->PopulateLoadTimeData(source);
  source->AddLocalizedString("appName", IDS_HELP_APP_EXPLORE);

  source->UseStringsJs();
  source->AddFrameAncestor(GURL(kChromeUIHelpAppURL));

  // TODO(https://crbug.com/1085328): Audit and tighten CSP.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' chrome-untrusted://help-app-kids-magazine;");
  return source;
}

content::WebUIDataSource* CreateHelpAppKidsMagazineUntrustedDataSource() {
  content::WebUIDataSource* source = content::WebUIDataSource::Create(
      kChromeUIHelpAppKidsMagazineUntrustedURL);
  // Set index.html as the default resource.
  source->SetDefaultResource(IDR_HELP_APP_KIDS_MAGAZINE_INDEX_HTML);
  source->DisableTrustedTypesCSP();

  // While the JS and CSS file are stored in /kids_magazine/static/..., the HTML
  // file references /static/... directly. We need to strip the "kids_magazine"
  // prefix from the path.
  for (size_t i = 0; i < kChromeosHelpAppKidsMagazineBundleResourcesSize; i++) {
    source->AddResourcePath(
        StripPrefix(kChromeosHelpAppKidsMagazineBundleResources[i].path,
                    "kids_magazine/"),
        kChromeosHelpAppKidsMagazineBundleResources[i].id);
  }

  // Add chrome://help-app and chrome-untrusted://help-app as frame ancestors.
  source->AddFrameAncestor(GURL(kChromeUIHelpAppURL));
  source->AddFrameAncestor(GURL(kChromeUIHelpAppUntrustedURL));
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc, "");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src 'self' https://www.gstatic.com;");
  return source;
}

}  // namespace chromeos
