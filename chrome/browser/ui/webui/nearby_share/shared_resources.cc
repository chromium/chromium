// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/nearby_share/shared_resources.h"

#include <string>

#include "base/containers/span.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/nearby_share_dialog_resources.h"
#include "chrome/grit/nearby_share_dialog_resources_map.h"
#include "ui/base/webui/web_ui_util.h"

const char kNearbyShareGeneratedPath[] =
    "@out_folder@/gen/chrome/browser/resources/nearby_share/";

void RegisterNearbySharedMojoResources(content::WebUIDataSource* data_source) {
  data_source->AddResourcePath("mojo/nearby_share_settings.mojom-lite.js",
                               IDR_NEARBY_SHARE_SETTINGS_MOJOM_LITE_JS);
}

void RegisterNearbySharedResources(content::WebUIDataSource* data_source) {
  std::string generate_path{kNearbyShareGeneratedPath};
  for (const GritResourceMap& resource : base::make_span(
           kNearbyShareDialogResources, kNearbyShareDialogResourcesSize)) {
    std::string path = resource.name;
    if (path.rfind(generate_path, 0) == 0) {
      path = path.substr(generate_path.size());
    }
    data_source->AddResourcePath(path, resource.value);
  }
  RegisterNearbySharedMojoResources(data_source);
}

void RegisterNearbySharedStrings(content::WebUIDataSource* data_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"secureConnectionId", IDS_NEARBY_SECURE_CONNECTION_ID},
      {"nearbyShareFeatureName", IDS_NEARBY_SHARE_FEATURE_NAME},
  };
  webui::AddLocalizedStringsBulk(data_source, kLocalizedStrings);
}
