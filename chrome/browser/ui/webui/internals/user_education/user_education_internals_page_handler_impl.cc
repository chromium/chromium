// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"

#include "chrome/grit/dev_ui_browser_resources.h"
#include "ui/base/webui/resource_path.h"

UserEducationInternalsPageHandlerImpl::UserEducationInternalsPageHandlerImpl(
    Profile* profile)
/* : profile_(profile) */ {
  // TODO(crbug.com/1194751): get reference to tutorial registry (once
  // implemented).
}

UserEducationInternalsPageHandlerImpl::
    ~UserEducationInternalsPageHandlerImpl() = default;

// static
void UserEducationInternalsPageHandlerImpl::AddPageResources(
    content::WebUIDataSource* source) {
  static const webui::ResourcePath kResources[] = {
      {"user-education", IDR_USER_EDUCATION_INTERNALS_HTML},
      {"user_education_internals.js", IDR_USER_EDUCATION_INTERNALS_JS},
      {"chrome/browser/ui/webui/internals/user_education/"
       "user_education_internals.mojom-webui.js",
       IDR_USER_EDUCATION_INTERNALS_MOJOM_WEBUI_JS},
  };

  source->DisableTrustedTypesCSP();
  source->AddResourcePaths(kResources);
}
