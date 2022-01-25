// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/webui/resource_path.h"

namespace {

TutorialService* GetTutorialService(Profile* profile) {
  auto* service = UserEducationServiceFactory::GetForProfile(profile);
  return service ? &service->tutorial_service() : nullptr;
}

}  // namespace

UserEducationInternalsPageHandlerImpl::UserEducationInternalsPageHandlerImpl(
    Profile* profile)
    : tutorial_service_(GetTutorialService(profile)), profile_(profile) {}

UserEducationInternalsPageHandlerImpl::
    ~UserEducationInternalsPageHandlerImpl() = default;

void UserEducationInternalsPageHandlerImpl::GetTutorials(
    GetTutorialsCallback callback) {
  std::vector<std::string> ids;
  if (tutorial_service_)
    ids = tutorial_service_->tutorial_registry()->GetTutorialIdentifiers();

  std::vector<std::string> tutorial_string_ids;
  for (const auto& id : ids)
    tutorial_string_ids.emplace_back(std::string(id));
  std::move(callback).Run(std::move(tutorial_string_ids));
}

void UserEducationInternalsPageHandlerImpl::StartTutorial(
    const std::string& tutorial_id) {
  CHECK(tutorial_service_);
  const ui::ElementContext context =
      chrome::FindBrowserWithProfile(profile_)->window()->GetElementContext();
  tutorial_service_->StartTutorial(tutorial_id, context);
}
