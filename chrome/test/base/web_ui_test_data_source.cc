// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/web_ui_test_data_source.h"

#include "chrome/common/webui_url_constants.h"
#include "chrome/test/data/grit/webui_generated_test_resources.h"
#include "chrome/test/data/grit/webui_generated_test_resources_map.h"
#include "content/public/browser/web_ui_data_source.h"

namespace webui {

content::WebUIDataSource* CreateWebUITestDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIWebUITestHost);
  source->AddResourcePaths(base::make_span(kWebuiGeneratedTestResources,
                                           kWebuiGeneratedTestResourcesSize));

  return source;
}

}  // namespace webui
