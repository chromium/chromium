// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler.h"

#include <string>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_handler_utils.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_metrics_recorder.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {

AddSupervisionHandler::AddSupervisionHandler(
    mojo::PendingReceiver<add_supervision::mojom::AddSupervisionHandler>
        receiver,
    content::WebUI* web_ui,
    signin::IdentityManager* identity_manager,
    Delegate* delegate)
    : web_ui_(web_ui),
      identity_manager_(identity_manager),
      receiver_(this, std::move(receiver)),
      delegate_(delegate) {}

AddSupervisionHandler::~AddSupervisionHandler() = default;

void AddSupervisionHandler::RequestClose(RequestCloseCallback callback) {
  bool dialog_closed = delegate_->CloseDialog();
  std::move(callback).Run(dialog_closed);
}

void AddSupervisionHandler::GetInstalledArcApps(
    GetInstalledArcAppsCallback callback) {
  Profile* profile = Profile::FromWebUI(web_ui_);
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);

  if (arc::ArcSessionManager::Get() == nullptr) {
    DLOG(WARNING) << "No ArcSessionManager available";
    std::move(callback).Run({});
    return;
  }
  if (arc::ArcSessionManager::Get()->state() !=
      arc::ArcSessionManager::State::ACTIVE) {
    DLOG(WARNING) << "ArcSessionManager is not active";
    std::move(callback).Run({});
    return;
  }

  std::vector<std::string> installed_arc_apps;

  proxy->AppRegistryCache().ForEachApp(
      [&installed_arc_apps, profile](const apps::AppUpdate& update) {
        // We don't include "sticky" ARC apps because they are system-required
        // apps that should not be offered for uninstallation. TODO(danan):
        // check for stickyness via the App Service instead when that is
        // available. (https://crbug.com/948408).
        if (ShouldIncludeAppUpdate(update) &&
            !arc::IsArcAppSticky(update.AppId(), profile)) {
          std::string package_name =
              arc::AppIdToArcPackageName(update.AppId(), profile);
          if (!package_name.empty())
            installed_arc_apps.push_back(package_name);
        }
      });

  std::move(callback).Run(installed_arc_apps);
}

void AddSupervisionHandler::GetOAuthToken(GetOAuthTokenCallback callback) {
  identity::ScopeSet scopes;
  scopes.insert(GaiaConstants::kKidsSupervisionSetupChildOAuth2Scope);
  scopes.insert(GaiaConstants::kPeopleApiReadOnlyOAuth2Scope);
  scopes.insert(GaiaConstants::kAccountsReauthOAuth2Scope);
  scopes.insert(GaiaConstants::kAuditRecordingOAuth2Scope);
  scopes.insert(GaiaConstants::kClearCutOAuth2Scope);

  oauth2_access_token_fetcher_ =
      identity_manager_->CreateAccessTokenFetcherForAccount(
          identity_manager_->GetPrimaryAccountId(), "add_supervision", scopes,
          base::BindOnce(&AddSupervisionHandler::OnAccessTokenFetchComplete,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          signin::AccessTokenFetcher::Mode::kImmediate);
}

void AddSupervisionHandler::LogOut() {
  LogOutHelper();
}

void AddSupervisionHandler::NotifySupervisionEnabled() {
  SupervisedUserService* service =
      SupervisedUserServiceFactory::GetForProfile(Profile::FromWebUI(web_ui_));
  service->set_signout_required_after_supervision_enabled();

  // Force full sign-in the next time the user is at the login screen.
  // Gellerization can only be triggered by the primary user.
  user_manager::UserManager* manager = user_manager::UserManager::Get();
  manager->SaveForceOnlineSignin(manager->GetPrimaryUser()->GetAccountId(),
                                 true /* force signin */);

  // Record UMA metric that user has completed Add Supervision process.
  AddSupervisionMetricsRecorder::GetInstance()->RecordAddSupervisionEnrollment(
      AddSupervisionMetricsRecorder::EnrollmentState::kCompleted);
}

void AddSupervisionHandler::OnAccessTokenFetchComplete(
    GetOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth2_access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    DLOG(ERROR) << "AddSupervisionHandler: OAuth2 token request failed. "
                << error.state() << ": " << error.ToString();

    std::move(callback).Run(
        add_supervision::mojom::OAuthTokenFetchStatus::ERROR, "");

  } else {
    std::move(callback).Run(add_supervision::mojom::OAuthTokenFetchStatus::OK,
                            access_token_info.token);
  }
}

}  // namespace chromeos
