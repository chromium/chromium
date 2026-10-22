// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/projector/projector_app_client_impl.h"

#include <optional>
#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator.h"
#include "chrome/browser/ui/ash/account_manager/account_manager_dialog_coordinator_factory.h"
#include "chrome/browser/ui/ash/projector/projector_soda_installation_controller.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_manager_core/account_manager_metrics.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/soda/constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace {

constexpr char kUsEnglishLocale[] = "en-US";

// Launches account reauthentication dialog for provided `email`.
// Note: the added/reauthenticated account may not match the account provided
// in the `email` field if user decided to edit the email inside the dialog.
void ShowProjectorAccountReauthDialog(content::BrowserContext* browser_context,
                                      const std::string& email) {
  CHECK(browser_context);

  ash::AccountManagerDialogCoordinatorFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context))
      ->ShowReauthAccountDialog(
          account_manager::AccountAdditionSource::kChromeOSProjectorAppReauth,
          email, base::DoNothing());
}

}  // namespace

// static
void ProjectorAppClientImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      ash::prefs::kProjectorCreationFlowEnabled, /*default_value=*/false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterStringPref(
      ash::prefs::kProjectorCreationFlowLanguage,
      /*default_value=*/kUsEnglishLocale,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      ash::prefs::kProjectorGalleryOnboardingShowCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterIntegerPref(
      ash::prefs::kProjectorViewerOnboardingShowCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterBooleanPref(ash::prefs::kProjectorAllowByPolicy,
                                /*default_value=*/true);
  registry->RegisterBooleanPref(
      ash::prefs::kProjectorDogfoodForFamilyLinkEnabled,
      /*default_value=*/false);
  registry->RegisterBooleanPref(
      ash::prefs::kProjectorExcludeTranscriptDialogShown,
      /*default_value=*/false);
  registry->RegisterBooleanPref(
      ash::prefs::kProjectorSWAUIPrefsMigrated, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
}

ProjectorAppClientImpl::ProjectorAppClientImpl(
    PrefService* local_state,
    ApplicationLocaleStorage* application_locale_storage)
    : local_state_(CHECK_DEREF(local_state)),
      application_locale_storage_(CHECK_DEREF(application_locale_storage)),
      pending_screencast_manager_(base::BindRepeating(
          &ProjectorAppClientImpl::NotifyScreencastsPendingStatusChanged,
          base::Unretained(this))) {}

ProjectorAppClientImpl::~ProjectorAppClientImpl() = default;

void ProjectorAppClientImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ProjectorAppClientImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ProjectorAppClientImpl::IsActiveAccount(
    const AccountId& account_id) const {
  if (!account_id.is_valid() || !user_manager::UserManager::IsInitialized()) {
    return false;
  }
  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  return active_user && active_user->GetAccountId() == account_id;
}

void ProjectorAppClientImpl::OnNewScreencastPreconditionChanged(
    const ash::NewScreencastPrecondition& precondition) {
  for (auto& observer : observers_) {
    if (IsActiveAccount(observer.GetAccountId())) {
      observer.OnNewScreencastPreconditionChanged(precondition);
    }
  }
}

const ash::PendingScreencastContainerSet&
ProjectorAppClientImpl::GetPendingScreencasts(
    const AccountId& account_id) const {
  static const base::NoDestructor<ash::PendingScreencastContainerSet>
      kEmptyScreencasts;
  if (!IsActiveAccount(account_id) ||
      pending_screencast_manager_.account_id() != account_id) {
    return *kEmptyScreencasts;
  }
  return pending_screencast_manager_.GetPendingScreencasts();
}

void ProjectorAppClientImpl::NotifyScreencastsPendingStatusChanged(
    const ash::PendingScreencastContainerSet& pending_screencast_containers) {
  const AccountId& observed_account = pending_screencast_manager_.account_id();
  if (!IsActiveAccount(observed_account)) {
    return;
  }
  for (auto& observer : observers_) {
    if (observer.GetAccountId() == observed_account) {
      observer.OnScreencastsPendingStatusChanged(pending_screencast_containers);
    }
  }
}

bool ProjectorAppClientImpl::ShouldDownloadSoda() const {
  return ProjectorSodaInstallationController::ShouldDownloadSoda(
      speech::GetLanguageCode(
          application_locale_storage_->GetTag().tag_string()));
}

void ProjectorAppClientImpl::InstallSoda() {
  return ProjectorSodaInstallationController::InstallSoda(
      *local_state_,
      std::string(application_locale_storage_->GetTag().tag_string()));
}

void ProjectorAppClientImpl::OnSodaInstallProgress(int combined_progress) {
  for (auto& observer : observers_) {
    if (IsActiveAccount(observer.GetAccountId())) {
      observer.OnSodaProgress(combined_progress);
    }
  }
}

void ProjectorAppClientImpl::OnSodaInstallError() {
  for (auto& observer : observers_) {
    if (IsActiveAccount(observer.GetAccountId())) {
      observer.OnSodaError();
    }
  }
}

void ProjectorAppClientImpl::OnSodaInstalled() {
  for (auto& observer : observers_) {
    if (IsActiveAccount(observer.GetAccountId())) {
      observer.OnSodaInstalled();
    }
  }
}

void ProjectorAppClientImpl::OpenFeedbackDialog(
    const AccountId& account_id) const {
  if (!IsActiveAccount(account_id)) {
    return;
  }
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id);
  if (!context) {
    return;
  }
  constexpr char kProjectorAppFeedbackCategoryTag[] = "FromProjectorApp";
  chrome::ShowFeedbackPage(GURL(ash::kChromeUIUntrustedProjectorUrl),
                           Profile::FromBrowserContext(context),
                           feedback::kFeedbackSourceProjectorApp,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/std::string(),
                           kProjectorAppFeedbackCategoryTag,
                           /*extra_diagnostics=*/std::string());
  // TODO(crbug/1048368): Communicate the dialog failing to open by returning an
  // error string. For now, assume that the dialog has opened successfully.
}

void ProjectorAppClientImpl::GetVideo(
    const AccountId& account_id,
    const std::string& video_file_id,
    const std::optional<std::string>& resource_key,
    ash::ProjectorAppClient::OnGetVideoCallback callback) const {
  if (!IsActiveAccount(account_id)) {
    std::move(callback).Run(
        ash::projector::mojom::GetVideoResult::NewErrorMessage(
            "Cannot fetch video for inactive user session"));
    return;
  }
  screencast_manager_.GetVideo(account_id, video_file_id, resource_key,
                               std::move(callback));
}

void ProjectorAppClientImpl::NotifyAppUIActive(const AccountId& account_id,
                                               bool active) {
  if (!IsActiveAccount(account_id)) {
    return;
  }
  if (pending_screencast_manager_.account_id() == account_id) {
    pending_screencast_manager_.OnAppActiveStatusChanged(active);
  }
  if (!active) {
    screencast_manager_.ResetScopeSuppressDriveNotifications();
  }
}

void ProjectorAppClientImpl::ToggleFileSyncingNotificationForPaths(
    const std::vector<base::FilePath>& screencast_paths,
    bool suppress) {
  pending_screencast_manager_.ToggleFileSyncingNotificationForPaths(
      screencast_paths, suppress);
}

void ProjectorAppClientImpl::HandleAccountReauth(const AccountId& account_id,
                                                 const std::string& email) {
  if (!IsActiveAccount(account_id)) {
    return;
  }
  content::BrowserContext* context =
      ash::BrowserContextHelper::Get()->GetBrowserContextByAccountId(
          account_id);
  if (!context) {
    return;
  }
  ShowProjectorAccountReauthDialog(context, email);
}
