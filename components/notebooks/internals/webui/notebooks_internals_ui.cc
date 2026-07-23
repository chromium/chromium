// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internals/webui/notebooks_internals_ui.h"

#include "components/grit/notebooks_internals_resources.h"
#include "components/grit/notebooks_internals_resources_map.h"
#include "components/notebooks/public/notebooks_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

namespace notebooks {

NotebooksInternalsUI::NotebooksInternalsUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, true) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      notebooks::kChromeUINotebooksInternalsHost);
  webui::SetupWebUIDataSource(source, kNotebooksInternalsResources,
                              IDR_NOTEBOOKS_INTERNALS_NOTEBOOKS_INTERNALS_HTML);
}

NotebooksInternalsUI::~NotebooksInternalsUI() = default;

WEB_UI_CONTROLLER_TYPE_IMPL(NotebooksInternalsUI)

}  // namespace notebooks
