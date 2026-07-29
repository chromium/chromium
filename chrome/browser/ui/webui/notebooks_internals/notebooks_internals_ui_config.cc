// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/notebooks_internals/notebooks_internals_ui_config.h"

#include "chrome/browser/notebooks/notebooks_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/notebooks/internals/webui/notebooks_internals_ui.h"
#include "components/notebooks/public/notebooks_constants.h"

namespace notebooks {

NotebooksInternalsUIConfig::NotebooksInternalsUIConfig()
    : InternalWebUIConfig(kChromeUINotebooksInternalsHost) {}

NotebooksInternalsUIConfig::~NotebooksInternalsUIConfig() = default;

std::unique_ptr<content::WebUIController>
NotebooksInternalsUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                                  const GURL& url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  return std::make_unique<NotebooksInternalsUI>(
      web_ui, NotebooksEligibilityServiceFactory::GetForProfile(profile));
}

}  // namespace notebooks
