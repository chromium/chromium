// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/data_sharing/data_sharing_page_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/views/data_sharing/data_sharing_utils.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_constants.h"

namespace {
constexpr base::TimeDelta kTokenRefreshTimeBuffer = base::Seconds(10);
#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
constexpr base::TimeDelta kDummyTokenExpirationDuration = base::Minutes(1);
#endif
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

DataSharingPageHandler::~DataSharingPageHandler() {}

void DataSharingPageHandler::ShowUI() {
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}

void DataSharingPageHandler::CloseUI(int status_code) {
  // TODO(crbug.com/368634445): In addition to closing the WebUI bubble some special
  // codes should trigger follow up native info dialogs.
  auto embedder = webui_controller_->embedder();
  if (embedder) {
    embedder->CloseUI();
  }
}

void DataSharingPageHandler::ApiInitComplete() {
  api_initialized_ = true;
  webui_controller_->ApiInitComplete();
}

void DataSharingPageHandler::GetShareLink(const std::string& group_id,
                                          const std::string& access_token,
                                          GetShareLinkCallback callback) {
  std::move(callback).Run(
      data_sharing::GetShareLink(group_id, access_token, GetProfile()));
}

void DataSharingPageHandler::AssociateTabGroupWithGroupId(
    const std::string& tab_group_id,
    const std::string& group_id) {
  data_sharing::AssociateTabGroupWithGroupId(tab_group_id, group_id,
                                             GetProfile());
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
      account_id, /*oauth_consumer_name=*/"data_sharing", /*scopes=*/
      {GaiaConstants::kPeopleApiReadWriteOAuth2Scope,
       GaiaConstants::kPeopleApiReadOnlyOAuth2Scope,
       GaiaConstants::kClearCutOAuth2Scope},
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

  base::TimeDelta time_delta = access_token_info.expiration_time -
                               base::Time::Now() - kTokenRefreshTimeBuffer;

  if (time_delta.is_positive()) {
    access_token_refresh_timer_->Start(
        FROM_HERE, time_delta, this,
        &DataSharingPageHandler::RequestAccessToken);
  } else {
    LOG(ERROR) << "Access token refresh time should not be negative or zero: "
                  "TimeDelta="
               << time_delta;
  }
}

void DataSharingPageHandler::ReadGroups(
    std::vector<std::string> group_ids,
    data_sharing::mojom::Page::ReadGroupsCallback callback) {
  CHECK(api_initialized_);
  page_->ReadGroups(group_ids, std::move(callback));
}

void DataSharingPageHandler::DeleteGroup(
    std::string group_id,
    data_sharing::mojom::Page::DeleteGroupCallback callback) {
  CHECK(api_initialized_);
  page_->DeleteGroup(group_id, std::move(callback));
}
