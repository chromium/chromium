// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/projector_message_handler.h"

#include <memory>

#include "ash/public/cpp/projector/projector_controller.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "content/public/browser/web_ui.h"

namespace chromeos {

namespace {

constexpr char kUserName[] = "name";
constexpr char kUserEmail[] = "email";
constexpr char kUserPictureURL[] = "pictureURL";
constexpr char kIsPrimaryUser[] = "isPrimaryUser";
constexpr char kToken[] = "token";
constexpr char kExpirationTime[] = "expirationTime";
constexpr char kError[] = "error";
constexpr char kOAuthTokenInfo[] = "oauthTokenInfo";
constexpr char kNoneStr[] = "NONE";
constexpr char kOtherStr[] = "OTHER";
constexpr char kTokenFetchFailureStr[] = "TOKEN_FETCH_FAILURE";

base::Value AccessTokenInfoToValue(const signin::AccessTokenInfo& info) {
  base::Value value(base::Value::Type::DICTIONARY);
  value.SetKey(kToken, base::Value(info.token));
  value.SetKey(kExpirationTime, base::TimeToValue(info.expiration_time));
  return value;
}

std::string ProjectorErrorToString(ProjectorError mode) {
  switch (mode) {
    case ProjectorError::kNone:
      return kNoneStr;
    case ProjectorError::kTokenFetchFailure:
      return kTokenFetchFailureStr;
    case ProjectorError::kOther:
      return kOtherStr;
  }
}

}  // namespace

ProjectorMessageHandler::ProjectorMessageHandler()
    : content::WebUIMessageHandler() {
  ProjectorAppClient::Get()->AddObserver(this);
}

ProjectorMessageHandler::~ProjectorMessageHandler() {
  ProjectorAppClient::Get()->RemoveObserver(this);
}

base::WeakPtr<ProjectorMessageHandler> ProjectorMessageHandler::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ProjectorMessageHandler::RegisterMessages() {
  web_ui()->RegisterDeprecatedMessageCallback(
      "getAccounts", base::BindRepeating(&ProjectorMessageHandler::GetAccounts,
                                         base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "canStartProjectorSession",
      base::BindRepeating(&ProjectorMessageHandler::CanStartProjectorSession,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "startProjectorSession",
      base::BindRepeating(&ProjectorMessageHandler::StartProjectorSession,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "getOAuthTokenForAccount",
      base::BindRepeating(&ProjectorMessageHandler::GetOAuthTokenForAccount,
                          base::Unretained(this)));

  web_ui()->RegisterDeprecatedMessageCallback(
      "onError", base::BindRepeating(&ProjectorMessageHandler::OnError,
                                     base::Unretained(this)));
}

void ProjectorMessageHandler::OnScreencastsStateChange() {}

void ProjectorMessageHandler::GetAccounts(const base::ListValue* args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args->GetList().size(), 1u);
  auto* controller = ash::ProjectorController::Get();
  DCHECK(controller);

  const std::vector<AccountInfo> accounts = oauth_token_fetcher_.GetAccounts();
  const CoreAccountInfo primary_account =
      oauth_token_fetcher_.GetPrimaryAccountInfo();

  std::vector<base::Value> response;
  response.reserve(accounts.size());
  for (const auto& info : accounts) {
    base::Value account_info(base::Value::Type::DICTIONARY);
    account_info.SetKey(kUserName, base::Value(info.full_name));
    account_info.SetKey(kUserEmail, base::Value(info.email));
    account_info.SetKey(kUserPictureURL, base::Value(info.picture_url));
    account_info.SetKey(kIsPrimaryUser,
                        base::Value(info.gaia == primary_account.gaia));
    response.push_back(std::move(account_info));
  }

  ResolveJavascriptCallback(args->GetList()[0],
                            base::Value(std::move(response)));
}

void ProjectorMessageHandler::CanStartProjectorSession(
    const base::ListValue* args) {
  AllowJavascript();

  // Check that there is only one argument which is the callback id.
  DCHECK_EQ(args->GetList().size(), 1u);

  ResolveJavascriptCallback(
      args->GetList()[0],
      base::Value(ash::ProjectorController::Get()->CanStartNewSession()));
}

void ProjectorMessageHandler::StartProjectorSession(
    const base::ListValue* args) {
  AllowJavascript();

  // There are two arguments. The first is the callback and the second is a list
  // containing the account which we need to start the recording with.
  DCHECK_EQ(args->GetList().size(), 2u);

  const auto& func_args = args->GetList()[1];
  DCHECK(func_args.is_list());

  // The first entry is the drive directory to save the screen cast to.
  // TODO(b/177959166): Pass the directory to ProjectorController when starting
  // a new session.
  DCHECK_EQ(func_args.GetList().size(), 1u);

  // TODO(b/195113693): Start the projector session with the selected account
  // and folder.
  auto* controller = ash::ProjectorController::Get();
  if (!controller->CanStartNewSession()) {
    ResolveJavascriptCallback(args->GetList()[0], base::Value(false));
    return;
  }

  controller->StartProjectorSession(args->GetList()[0].GetString());
  ResolveJavascriptCallback(args->GetList()[0], base::Value(true));
}

void ProjectorMessageHandler::GetOAuthTokenForAccount(
    const base::ListValue* args) {
  // Two arguments. The first is callback id, and the second is the list
  // containing the account for which to fetch the oauth token.
  DCHECK_EQ(args->GetList().size(), 2u);

  const auto& requested_account = args->GetList()[1];
  DCHECK(requested_account.is_list());
  DCHECK_EQ(requested_account.GetList().size(), 1u);

  auto& oauth_token_fetch_callback = args->GetList()[0].GetString();
  const std::string& email = requested_account.GetList()[0].GetString();

  oauth_token_fetcher_.GetAccessTokenFor(
      email,
      base::BindOnce(&ProjectorMessageHandler::OnAccessTokenRequestCompleted,
                     GetWeakPtr(), oauth_token_fetch_callback));
}

void ProjectorMessageHandler::OnError(const base::ListValue* args) {
  // TODO(b/195113693): Get the SWA dialog associated with this WebUI and close
  // it.
}

void ProjectorMessageHandler::OnAccessTokenRequestCompleted(
    const std::string& js_callback_id,
    const std::string& email,
    GoogleServiceAuthError error,
    const signin::AccessTokenInfo& info) {
  AllowJavascript();

  base::Value response(base::Value::Type::DICTIONARY);
  response.SetKey(kUserEmail, base::Value(email));
  if (error.state() != GoogleServiceAuthError::State::NONE) {
    response.SetKey(kOAuthTokenInfo, base::Value());
    response.SetKey(kError, base::Value(ProjectorErrorToString(
                                ProjectorError::kTokenFetchFailure)));
  } else {
    response.SetKey(kError,
                    base::Value(ProjectorErrorToString(ProjectorError::kNone)));
    response.SetKey(kOAuthTokenInfo, AccessTokenInfoToValue(info));
  }

  ResolveJavascriptCallback(base::Value(std::move(js_callback_id)),
                            std::move(response));
}

}  // namespace chromeos
