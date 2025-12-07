// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace {
constexpr base::TimeDelta kTokenRetryTimeDelta = base::Seconds(1);
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr base::TimeDelta kDummyTokenExpirationDuration = base::Minutes(1);
#endif

tab_groups::TabGroupId ParseTabGroupIdFromString(const std::string& value) {
  std::optional<base::Token> token = base::Token::FromString(value);
  CHECK(token);
  return tab_groups::TabGroupId::FromRawToken(token.value());
}

}  // namespace

DataSharingPageHandler::DataSharingPageHandler(
    DataSharingUI* webui_controller,
    mojo::PendingReceiver<data_sharing::mojom::PageHandler> receiver,
    mojo::PendingRemote<data_sharing::mojom::Page> page)
    : webui_controller_(webui_controller),
      access_token_refresh_timer_(std::make_unique<base::OneShotTimer>()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  RequestAccessToken();
}

DataSharingPageHandler::~DataSharingPageHandler() = default;

bool DataSharingPageHandler::IsApiInitialized() {
  return api_initialized_;
}

void DataSharingPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void DataSharingPageHandler::CloseUI(int status_code) {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }

  if (absl::StatusCode(status_code) != absl::StatusCode::kOk) {
    webui_controller_->ShowErrorDialog(status_code);
  }
}

void DataSharingPageHandler::ApiInitComplete() {
  api_initialized_ = true;
  webui_controller_->ApiInitComplete();
}

void DataSharingPageHandler::MakeTabGroupShared(
    const std::string& tab_group_id,
    const std::string& group_id,
    const std::string& access_token,
    MakeTabGroupSharedCallback callback) {
  // This call is only allowed to call once per lifetime of this class.
  // All subsequent calls should go through GetSharedLink instead.
  // TODO(crbug.com/396133860): Replace CHECK with call to terminate the
  // renderer instead.
  CHECK(!has_made_tab_group_shared_);
  has_made_tab_group_shared_ = true;
  webui_controller_->OnShareLinkRequested(group_id, access_token,
                                          std::move(callback));
}

void DataSharingPageHandler::GetShareLink(const std::string& group_id,
                                          const std::string& access_token,
                                          GetShareLinkCallback callback) {
  std::move(callback).Run(
      data_sharing::GetShareLink(group_id, access_token, GetProfile()));
}

void DataSharingPageHandler::GetTabGroupPreview(
    const std::string& group_id,
    const std::string& access_token,
    GetTabGroupPreviewCallback callback) {
  data_sharing::GetTabGroupPreview(group_id, access_token, GetProfile(),
                                   std::move(callback));
}

void DataSharingPageHandler::OpenTabGroup(const std::string& group_id) {
}

void DataSharingPageHandler::AboutToUnShareTabGroup(
    const std::string& tab_group_id) {
  tab_groups::TabGroupId local_tab_group_id =
      ParseTabGroupIdFromString(tab_group_id);
  // TODO(crbug.com/399961647): Prefer to wait for the callback to complete.
  tab_groups::TabGroupSyncServiceFactory::GetForProfile(GetProfile())
      ->AboutToUnShareTabGroup(local_tab_group_id, base::DoNothing());
}

void DataSharingPageHandler::OnTabGroupUnShareComplete(
    const std::string& tab_group_id) {
  tab_groups::TabGroupId local_tab_group_id =
      ParseTabGroupIdFromString(tab_group_id);
  tab_groups::TabGroupSyncServiceFactory::GetForProfile(GetProfile())
      ->OnTabGroupUnShareComplete(local_tab_group_id, /*success=*/true);
}

Profile* DataSharingPageHandler::GetProfile() {
  CHECK(webui_controller_);
  return Profile::FromWebUI(webui_controller_->web_ui());
}

void DataSharingPageHandler::RequestAccessToken() {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(GetProfile());
  CoreAccountId account_id =
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  access_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account_id, signin::OAuthConsumerId::kDataSharing,
      base::BindOnce(&DataSharingPageHandler::OnAccessTokenFetched,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
#else
  // For non-branded build return an empty access token to bypass the
  // authentication flow. Delay the call to make sure this class is accessible
  // through `DataSharingUI`.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DataSharingPageHandler::OnAccessTokenFetched,
          weak_ptr_factory_.GetWeakPtr(),
          GoogleServiceAuthError(GoogleServiceAuthError::NONE),
          signin::AccessTokenInfo(
              "", base::Time::Now() + kDummyTokenExpirationDuration, "")));
#endif
}

void DataSharingPageHandler::OnAccessTokenFetched(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  // It is safe to reset the token fetcher now.
  access_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    LOG(ERROR) << "Access token auth error: state=" << error.state();
  }
  // Note: We do not do anything special for empty tokens.
  page_->OnAccessTokenFetched(access_token_info.token);

  base::TimeDelta time_delta =
      access_token_info.expiration_time - base::Time::Now();

  // In case access token is expired, retry it later.
  if (!time_delta.is_positive()) {
    time_delta = kTokenRetryTimeDelta;
  }

  access_token_refresh_timer_->Start(
      FROM_HERE, time_delta, this, &DataSharingPageHandler::RequestAccessToken);
}

void DataSharingPageHandler::ReadGroups(
    data_sharing::mojom::ReadGroupsParamsPtr read_group_params,
    data_sharing::mojom::Page::ReadGroupsCallback callback) {
  CHECK(api_initialized_);
  page_->ReadGroups(std::move(read_group_params), std::move(callback));
}

void DataSharingPageHandler::DeleteGroup(
    std::string group_id,
    data_sharing::mojom::Page::DeleteGroupCallback callback) {
  CHECK(api_initialized_);
  page_->DeleteGroup(group_id, std::move(callback));
}

void DataSharingPageHandler::LeaveGroup(
    std::string group_id,
    data_sharing::mojom::Page::LeaveGroupCallback callback) {
  CHECK(api_initialized_);
  page_->LeaveGroup(group_id, std::move(callback));
}

void DataSharingPageHandler::ReadGroupWithToken(
    data_sharing::mojom::ReadGroupWithTokenParamPtr param,
    data_sharing::mojom::Page::ReadGroupWithTokenCallback callback) {
  CHECK(api_initialized_);
  page_->ReadGroupWithToken(std::move(param), std::move(callback));
}

void DataSharingPageHandler::OnGroupAction(
    data_sharing::mojom::GroupAction action,
    data_sharing::mojom::GroupActionProgress progress) {
  webui_controller_->OnGroupAction(action, progress);
}
