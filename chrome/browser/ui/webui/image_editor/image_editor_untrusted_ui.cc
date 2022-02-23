// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/image_editor/image_editor_untrusted_ui.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/webui/image_editor/image_editor_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/image_editor_untrusted_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"

namespace image_editor {

ImageEditorUntrustedUIConfig::ImageEditorUntrustedUIConfig()
    : WebUIConfig(content::kChromeUIUntrustedScheme,
                  chrome::kChromeUIImageEditorHost) {}

ImageEditorUntrustedUIConfig::~ImageEditorUntrustedUIConfig() = default;

std::unique_ptr<content::WebUIController>
ImageEditorUntrustedUIConfig::CreateWebUIController(content::WebUI* web_ui) {
  return std::make_unique<ImageEditorUntrustedUI>(web_ui);
}

ImageEditorUntrustedUI::ImageEditorUntrustedUI(content::WebUI* web_ui)
    : ui::UntrustedWebUIController(web_ui) {
  content::WebUIDataSource* untrusted_source = content::WebUIDataSource::Create(
      chrome::kChromeUIUntrustedImageEditorURL);
  untrusted_source->AddResourcePaths(base::make_span(
      kImageEditorUntrustedResources, kImageEditorUntrustedResourcesSize));
  untrusted_source->AddFrameAncestor(GURL(chrome::kChromeUIImageEditorURL));
}

ImageEditorUntrustedUI::~ImageEditorUntrustedUI() = default;

void ImageEditorUntrustedUI::BindInterface(
    mojo::PendingReceiver<mojom::ImageEditorHandler> pending_receiver) {
  // TODO(crbug.com/1297362): The lifetime of the WebUIController and the mojo
  // interface may vary. This requires supporting multiple binding.
  if (receiver_.is_bound())
    receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void ImageEditorUntrustedUI::RecordUserAction(mojom::EditAction action) {
  base::UmaHistogramEnumeration("Sharing.DesktopScreenshot.Action", action);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ImageEditorUntrustedUI)

}  // namespace image_editor
