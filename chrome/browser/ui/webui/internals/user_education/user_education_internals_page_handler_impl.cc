// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/internals/user_education/user_education_internals_page_handler_impl.h"

#include "chrome/browser/ui/user_education/feature_tutorial_service.h"
#include "chrome/browser/ui/user_education/feature_tutorial_service_factory.h"
#include "chrome/browser/ui/user_education/feature_tutorials.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "ui/base/webui/resource_path.h"

UserEducationInternalsPageHandlerImpl::UserEducationInternalsPageHandlerImpl(
    Profile* profile)
    : tutorial_service_(FeatureTutorialServiceFactory::GetForProfile(profile)) {
}

UserEducationInternalsPageHandlerImpl::
    ~UserEducationInternalsPageHandlerImpl() = default;

void UserEducationInternalsPageHandlerImpl::GetTutorials(
    GetTutorialsCallback callback) {
  std::vector<base::StringPiece> id_pieces = GetAllFeatureTutorialStringIds();

  std::vector<std::string> ids;
  for (base::StringPiece piece : id_pieces)
    ids.emplace_back(piece.data(), piece.size());

  std::move(callback).Run(std::move(ids));
}

void UserEducationInternalsPageHandlerImpl::StartTutorial(
    const std::string& tutorial_id) {
  absl::optional<FeatureTutorial> tutorial =
      GetFeatureTutorialFromStringId(tutorial_id);
  if (!tutorial)
    return;
  tutorial_service_->StartTutorial(*tutorial);
}
