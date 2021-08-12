// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image_editor/image_editor_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui_data_source.h"

ImageEditorUI::ImageEditorUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://image-editor source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIImageEditorHost);
  html_source->SetDefaultResource(IDR_IMAGE_EDITOR_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}
