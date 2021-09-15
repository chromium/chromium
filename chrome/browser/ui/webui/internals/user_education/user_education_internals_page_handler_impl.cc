// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"

#include "chrome/grit/dev_ui_browser_resources.h"
#include "ui/base/webui/resource_path.h"

UserEducationInternalsPageHandlerImpl::UserEducationInternalsPageHandlerImpl(
    Profile* profile) {}

UserEducationInternalsPageHandlerImpl::
    ~UserEducationInternalsPageHandlerImpl() = default;

void UserEducationInternalsPageHandlerImpl::GetTutorials(
    GetTutorialsCallback callback) {
  std::vector<std::string> ids;
  std::move(callback).Run(std::move(ids));
}

void UserEducationInternalsPageHandlerImpl::StartTutorial(
    const std::string& tutorial_id) {
  return;
}
