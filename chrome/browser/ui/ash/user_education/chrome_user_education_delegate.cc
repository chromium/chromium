// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/user_education/chrome_user_education_delegate.h"

#include "ash/ash_element_identifiers.h"
#include "ash/user_education/user_education_types.h"
#include "ash/user_education/user_education_util.h"
#include "base/check.h"
#include "base/values.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service.h"
#include "chrome/browser/ash/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/help_bubble_params.h"
#include "components/user_education/common/tutorial_registry.h"
#include "components/user_education/common/tutorial_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/interaction/element_tracker.h"

namespace {

// Helpers ---------------------------------------------------------------------

app_list::AppListSyncableService* GetAppListSyncableService(Profile* profile) {
  return app_list::AppListSyncableServiceFactory::GetForProfile(profile);
}

Profile* GetProfile(const AccountId& account_id) {
  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id));
}

bool IsPrimaryProfile(Profile* profile) {
  return user_manager::UserManager::Get()->IsPrimaryUser(
      ash::BrowserContextHelper::Get()->GetUserByBrowserContext(profile));
}

absl::optional<std::string> ToString(
    absl::optional<ash::TutorialId> tutorial_id) {
  return tutorial_id ? absl::make_optional(ash::user_education_util::ToString(
                           tutorial_id.value()))
                     : absl::nullopt;
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
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto* const profile = GetProfile(account_id);
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

const absl::optional<bool>& ChromeUserEducationDelegate::IsNewUser(
    const AccountId& account_id) const {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto* const profile = GetProfile(account_id);
  CHECK(IsPrimaryProfile(profile));
  return is_primary_profile_new_user_;
}

void ChromeUserEducationDelegate::RegisterTutorial(
    const AccountId& account_id,
    ash::TutorialId tutorial_id,
    user_education::TutorialDescription tutorial_description) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto* const profile = GetProfile(account_id);
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
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto* const profile = GetProfile(account_id);
  CHECK(IsPrimaryProfile(profile));
  UserEducationServiceFactory::GetForProfile(profile)
      ->tutorial_service()
      .StartTutorial(ash::user_education_util::ToString(tutorial_id),
                     std::move(element_context), std::move(completed_callback),
                     std::move(aborted_callback));
}

void ChromeUserEducationDelegate::AbortTutorial(
    const AccountId& account_id,
    absl::optional<ash::TutorialId> tutorial_id) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto* const profile = GetProfile(account_id);
  CHECK(IsPrimaryProfile(profile));

  auto& tutorial_service =
      UserEducationServiceFactory::GetForProfile(profile)->tutorial_service();
  tutorial_service.CancelTutorialIfRunning(ToString(tutorial_id));
}

void ChromeUserEducationDelegate::LaunchSystemWebAppAsync(
    const AccountId& account_id,
    ash::SystemWebAppType system_web_app_type,
    int64_t display_id) {
  // NOTE: User education in Ash is currently only supported for the primary
  // user profile. This is a self-imposed restriction.
  auto* const profile = GetProfile(account_id);
  CHECK(IsPrimaryProfile(profile));
  ash::LaunchSystemWebAppAsync(profile, system_web_app_type,
                               ash::SystemAppLaunchParams(),
                               std::make_unique<apps::WindowInfo>(display_id));
}

bool ChromeUserEducationDelegate::IsRunningTutorial(
    const AccountId& account_id,
    absl::optional<ash::TutorialId> tutorial_id) const {
  return UserEducationServiceFactory::GetForProfile(GetProfile(account_id))
      ->tutorial_service()
      .IsRunningTutorial(ToString(tutorial_id));
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

  // Cache whether the user associated with the primary profile is considered
  // new, based on whether the first app list sync in the session was the first
  // sync ever across all ChromeOS devices and sessions for the given user.
  if (auto* app_list_syncable_service = GetAppListSyncableService(profile)) {
    app_list_syncable_service->OnFirstSync(base::BindOnce(
        [](const base::WeakPtr<ChromeUserEducationDelegate>& self,
           bool was_first_sync_ever) {
          if (self) {
            self->is_primary_profile_new_user_ = was_first_sync_ever;
          }
        },
        weak_ptr_factory_.GetWeakPtr()));
  }
}

void ChromeUserEducationDelegate::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}
