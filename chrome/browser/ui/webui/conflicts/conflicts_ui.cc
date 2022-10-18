// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/conflicts/conflicts_ui.h"

#include <memory>

#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/conflicts/conflicts_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui_data_source.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

content::WebUIDataSource* CreateConflictsUIHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIConflictsHost);
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ScriptSrc,
      "script-src chrome://resources 'self';");

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types polymer-html-literal "
      "polymer-template-event-attribute-policy;");

  source->AddResourcePath("conflicts.js", IDR_ABOUT_CONFLICTS_JS);
  source->AddResourcePath("warning.svg", IDR_ABOUT_CONFLICTS_WARNING_SVG);
  source->SetDefaultResource(IDR_ABOUT_CONFLICTS_HTML);
  return source;
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
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateConflictsUIHTMLSource());
}

// static
base::RefCountedMemory* ConflictsUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_CONFLICT_FAVICON, scale_factor));
}
