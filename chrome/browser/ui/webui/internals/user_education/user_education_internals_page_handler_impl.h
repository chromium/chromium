// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_
#define CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom.h"
#include "content/public/browser/web_ui_data_source.h"

class UserEducationInternalsPageHandlerImpl
    : public mojom::user_education_internals::
          UserEducationInternalsPageHandler {
 public:
  explicit UserEducationInternalsPageHandlerImpl(Profile* profile);
  ~UserEducationInternalsPageHandlerImpl() override;

  UserEducationInternalsPageHandlerImpl(
      const UserEducationInternalsPageHandlerImpl&) = delete;
  UserEducationInternalsPageHandlerImpl& operator=(
      const UserEducationInternalsPageHandlerImpl&) = delete;

  // mojom::user_education_internals::UserEducationInternalsPageHandler:
  void GetTutorials(GetTutorialsCallback callback) override;
  void StartTutorial(const std::string& tutorial_id) override;

 private:
  TutorialService* tutorial_service_;
  Profile* profile_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_INTERNALS_USER_EDUCATION_USER_EDUCATION_INTERNALS_PAGE_HANDLER_IMPL_H_
