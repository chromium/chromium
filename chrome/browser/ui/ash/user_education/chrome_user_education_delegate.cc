// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include "ash/user_education/user_education_constants.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/interaction/element_tracker.h"

namespace {

// Helpers ---------------------------------------------------------------------

bool IsPrimaryProfile(Profile* profile) {
  return user_manager::UserManager::Get()->IsPrimaryUser(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile));
}

}  // namespace

// ChromeUserEducationDelegate -------------------------------------------------

ChromeUserEducationDelegate::ChromeUserEducationDelegate() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  profile_manager_observation_.Observe(profile_manager);
  for (Profile* profile : profile_manager->GetLoadedProfiles()) {
    OnProfileAdded(profile);
  }
}

ChromeUserEducationDelegate::~ChromeUserEducationDelegate() = default;

std::unique_ptr<user_education::HelpBubble>
ChromeUserEducationDelegate::CreateHelpBubble(
    const AccountId& account_id,
    ash::HelpBubbleId help_bubble_id,
    user_education::HelpBubbleParams help_bubble_params,
    ui::ElementIdentifier element_id,
    ui::ElementContext element_context) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));

  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  CHECK(IsPrimaryProfile(profile));

  // If a tracked `element` cannot be found for the specified `element_id` and
  // `element_context` pair, there's nothing to anchor a help bubble to.
  ui::TrackedElement* const element =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          element_id, element_context);
  if (!element) {
    return nullptr;
  }

  // Help bubble factories expect `help_bubble_id` to be provided via extended
  // properties being as it is a ChromeOS specific platform construct.
  help_bubble_params.extended_properties.values().Merge(std::move(
      ash::user_education_util::CreateExtendedProperties(help_bubble_id)
          .values()));

  return UserEducationServiceFactory::GetForProfile(profile)
      ->help_bubble_factory_registry()
      .CreateHelpBubble(element, std::move(help_bubble_params));
}

absl::optional<ui::ElementIdentifier>
ChromeUserEducationDelegate::GetElementIdentifierForAppId(
    const std::string& app_id) const {
  if (!strcmp(web_app::kHelpAppId, app_id.c_str())) {
    return ash::kExploreAppElementId;
  }
  if (!strcmp(web_app::kOsSettingsAppId, app_id.c_str())) {
    return ash::kSettingsAppElementId;
  }
  return absl::nullopt;
}

void ChromeUserEducationDelegate::RegisterTutorial(
    const AccountId& account_id,
    ash::TutorialId tutorial_id,
    user_education::TutorialDescription tutorial_description) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));

  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  CHECK(IsPrimaryProfile(profile));

  UserEducationServiceFactory::GetForProfile(profile)
      ->tutorial_registry()
      .AddTutorial(ash::user_education_util::ToString(tutorial_id),
                   std::move(tutorial_description));
}

void ChromeUserEducationDelegate::StartTutorial(
    const AccountId& account_id,
    ash::TutorialId tutorial_id,
    ui::ElementContext element_context,
    base::OnceClosure completed_callback,
    base::OnceClosure aborted_callback) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));

  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  CHECK(IsPrimaryProfile(profile));

  UserEducationServiceFactory::GetForProfile(profile)
      ->tutorial_service()
      .StartTutorial(ash::user_education_util::ToString(tutorial_id),
                     std::move(element_context), std::move(completed_callback),
                     std::move(aborted_callback));
}

void ChromeUserEducationDelegate::AbortTutorial(const AccountId& account_id) {
  Profile* profile = Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));

  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  CHECK(IsPrimaryProfile(profile));

  UserEducationServiceFactory::GetForProfile(profile)
      ->tutorial_service()
      .AbortTutorial(/*abort_step=*/absl::nullopt);
}

void ChromeUserEducationDelegate::OnProfileAdded(Profile* profile) {
  // NOTE: User eduction in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  if (!IsPrimaryProfile(profile)) {
    return;
  }

  // Since we only currently support the primary user profile, we can stop
  // observing the profile manager once it has been added.
  profile_manager_observation_.Reset();

  // Register tutorial dependencies.
  RegisterChromeHelpBubbleFactories(
      UserEducationServiceFactory::GetForProfile(profile)
          ->help_bubble_factory_registry());
}

void ChromeUserEducationDelegate::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}
