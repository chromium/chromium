// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/camera/camera_ui.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/system_web_app_manager.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/camera_resources.h"
#include "chrome/grit/camera_resources_map.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_ui_data_source.h"
#include "mojo/public/js/grit/mojo_bindings_resources.h"

namespace chromeos {

namespace {

content::WebUIDataSource* CreateCameraUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUICameraHost);

  // Add all settings resources.
  for (size_t i = 0; i < kCameraResourcesSize; ++i) {
    source->AddResourcePath(kCameraResources[i].name,
                            kCameraResources[i].value);
  }

  // Add WebUI version of the CCA browser proxy.
  source->AddResourcePath("src/js/browser_proxy/browser_proxy.js",
                          IDR_CAMERA_WEBUI_BROWSER_PROXY);

  // Add mojom-lite files under expected paths.
  source->AddResourcePath("src/js/mojo/camera_intent.mojom-lite.js",
                          IDR_CAMERA_CAMERA_INTENT_MOJOM_LITE_JS);
  source->AddResourcePath("src/js/mojo/image_capture.mojom-lite.js",
                          IDR_CAMERA_IMAGE_CAPTURE_MOJOM_LITE_JS);
  source->AddResourcePath("src/js/mojo/camera_common.mojom-lite.js",
                          IDR_CAMERA_CAMERA_COMMON_MOJOM_LITE_JS);
  source->AddResourcePath("src/js/mojo/camera_metadata.mojom-lite.js",
                          IDR_CAMERA_CAMERA_METADATA_MOJOM_LITE_JS);
  source->AddResourcePath("src/js/mojo/camera_metadata_tags.mojom-lite.js",
                          IDR_CAMERA_CAMERA_METADATA_TAGS_MOJOM_LITE_JS);
  source->AddResourcePath("src/js/mojo/camera_app.mojom-lite.js",
                          IDR_CAMERA_APP_MOJOM_LITE_JS);
  source->AddResourcePath("src/js/mojo/mojo_bindings_lite.js",
                          IDR_MOJO_MOJO_BINDINGS_LITE_JS);

  // Add System Web App resources.
  source->AddResourcePath("pwa.html", IDR_PWA_HTML);

  source->UseStringsJs();

  return source;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// CameraUI
//
///////////////////////////////////////////////////////////////////////////////

CameraUI::CameraUI(content::WebUI* web_ui) : ui::MojoWebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Set up the data source.
  content::WebUIDataSource* source = CreateCameraUIHTMLSource();
  content::WebUIDataSource::Add(profile, source);
}

CameraUI::~CameraUI() = default;

// static
bool CameraUI::IsEnabled() {
  return web_app::SystemWebAppManager::IsAppEnabled(
      web_app::SystemAppType::CAMERA);
}

}  // namespace chromeos
