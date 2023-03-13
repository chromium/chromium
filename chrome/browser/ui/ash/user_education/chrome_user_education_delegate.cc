// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/tutorial_registry.h"

ChromeUserEducationDelegate::ChromeUserEducationDelegate() = default;

ChromeUserEducationDelegate::~ChromeUserEducationDelegate() = default;

void ChromeUserEducationDelegate::RegisterTutorial(
    const AccountId& account_id,
    const std::string& tutorial_id,
    user_education::TutorialDescription tutorial_description) {
  UserEducationServiceFactory::GetForProfile(
      Profile::FromBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
              account_id)))
      ->tutorial_registry()
      .AddTutorial(tutorial_id, std::move(tutorial_description));
}
