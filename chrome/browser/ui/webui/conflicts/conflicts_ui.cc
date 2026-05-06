// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/conflicts/conflicts_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/conflicts_resources.h"
#include "chrome/grit/conflicts_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/webui/webui_util.h"

namespace {

void CreateAndAddConflictsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIConflictsHost);
  webui::SetupWebUIDataSource(source, kConflictsResources,
                              IDR_CONFLICTS_ABOUT_CONFLICTS_HTML);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ConflictsUI
//
///////////////////////////////////////////////////////////////////////////////

ConflictsUI::ConflictsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ConflictsHandler>());

  // Set up the about:conflicts source.
  CreateAndAddConflictsUIHTMLSource(Profile::FromWebUI(web_ui));
}

// static
base::RefCountedMemory* ConflictsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_CONFLICT_FAVICON, scale_factor));
}
