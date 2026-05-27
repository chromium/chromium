// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/default_browser/visual_guided_setter_ui.h"

#include "base/containers/span.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/visual_guided_setter_resources.h"
#include "chrome/grit/visual_guided_setter_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

VisualGuidedSetterUI::VisualGuidedSetterUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/false) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIDefaultBrowserVisualGuidedSetterHost);
  webui::SetupWebUIDataSource(
      source, base::span(kVisualGuidedSetterResources),
      IDR_VISUAL_GUIDED_SETTER_VISUAL_GUIDED_SETTER_HTML);
}

WEB_UI_CONTROLLER_TYPE_IMPL(VisualGuidedSetterUI)

VisualGuidedSetterUI::~VisualGuidedSetterUI() = default;
