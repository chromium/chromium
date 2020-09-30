// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/media_app_ui/media_app_guest_ui.h"

#include "chromeos/components/media_app_ui/media_app_ui_delegate.h"
#include "chromeos/components/media_app_ui/url_constants.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources.h"
#include "chromeos/grit/chromeos_media_app_bundle_resources_map.h"
#include "chromeos/grit/chromeos_media_app_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace chromeos {

content::WebUIDataSource* CreateMediaAppUntrustedDataSource(
    MediaAppUIDelegate* delegate) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(kChromeUIMediaAppGuestURL);
  // Add resources from chromeos_media_app_resources.pak.
  source->AddResourcePath("app.html", IDR_MEDIA_APP_APP_HTML);
  source->AddResourcePath("media_app_app_scripts.js",
                          IDR_MEDIA_APP_APP_SCRIPTS_JS);

  // Add resources from chromeos_media_app_bundle_resources.pak that are also
  // needed for mocks. If enable_cros_media_app = true, then these calls will
  // happen a second time with the same parameters. When false, we need these to
  // specify what routes are mocked by files in ./resources/mock/js. The loop is
  // irrelevant in that case.
  source->AddResourcePath("js/app_main.js", IDR_MEDIA_APP_APP_MAIN_JS);
  source->AddResourcePath("js/app_image_handler_module.js",
                          IDR_MEDIA_APP_APP_IMAGE_HANDLER_MODULE_JS);

  // Add all resources from chromeos_media_app_bundle_resources.pak.
  for (size_t i = 0; i < kChromeosMediaAppBundleResourcesSize; i++) {
    source->AddResourcePath(kChromeosMediaAppBundleResources[i].name,
                            kChromeosMediaAppBundleResources[i].value);
  }

  delegate->PopulateLoadTimeData(source);
  source->UseStringsJs();

  source->AddFrameAncestor(GURL(kChromeUIMediaAppURL));
  // By default, prevent all network access.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::DefaultSrc,
      "default-src blob: 'self';");
  // Need to explicitly set |worker-src| because CSP falls back to |child-src|
  // which is none.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::WorkerSrc, "worker-src 'self';");
  // Allow images to also handle data urls.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ImgSrc, "img-src blob: data: 'self';");
  // Allow styles to include inline styling needed for Polymer elements.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::StyleSrc,
      "style-src 'self' 'unsafe-inline';");
  // TODO(crbug.com/1098685): Trusted Type remaining WebUI.
  source->DisableTrustedTypesCSP();
  return source;
}

}  // namespace chromeos
