// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/tutorial/browser_tutorial_service_factory.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service_manager.h"
#include "ui/base/webui/resource_path.h"

UserEducationInternalsPageHandlerImpl::UserEducationInternalsPageHandlerImpl(
    Profile* profile)
    : tutorial_service_(
          TutorialServiceManager::GetInstance()->GetTutorialServiceForProfile(
              profile)),
      profile_(profile) {}

UserEducationInternalsPageHandlerImpl::
    ~UserEducationInternalsPageHandlerImpl() = default;

void UserEducationInternalsPageHandlerImpl::GetTutorials(
    GetTutorialsCallback callback) {
  std::vector<std::string> ids = tutorial_service_->GetTutorialIdentifiers();

  std::vector<std::string> tutorial_string_ids;
  for (const auto& id : ids) {
    tutorial_string_ids.emplace_back(std::string(id));
  }
  std::move(callback).Run(std::move(tutorial_string_ids));
}

void UserEducationInternalsPageHandlerImpl::StartTutorial(
    const std::string& tutorial_id) {
  ui::ElementContext context =
      BrowserTutorialServiceFactory::GetDefaultElementContextForProfile(
          profile_);

  tutorial_service_->StartTutorial(tutorial_id, context);
}
