// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_reauth_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/ui/signin_reauth_view_controller.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/webui/web_ui_util.h"

SigninReauthHandler::SigninReauthHandler(
    SigninReauthViewController* controller,
    base::flat_map<std::string, int> string_to_grd_id_map)
    : controller_(controller),
      string_to_grd_id_map_(std::move(string_to_grd_id_map)) {
  DCHECK(controller_);
  controller_observer_.Add(controller_);
}

SigninReauthHandler::~SigninReauthHandler() = default;

void SigninReauthHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "initialize", base::BindRepeating(&SigninReauthHandler::HandleInitialize,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "confirm", base::BindRepeating(&SigninReauthHandler::HandleConfirm,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "cancel", base::BindRepeating(&SigninReauthHandler::HandleCancel,
                                    base::Unretained(this)));
}

void SigninReauthHandler::OnJavascriptAllowed() {
  if (!controller_)
    return;

  SigninReauthViewController::GaiaReauthType gaia_reauth_type =
      controller_->gaia_reauth_type();
  if (gaia_reauth_type !=
      SigninReauthViewController::GaiaReauthType::kUnknown) {
    OnGaiaReauthTypeDetermined(gaia_reauth_type);
  }
}

void SigninReauthHandler::OnReauthControllerDestroyed() {
  controller_observer_.RemoveAll();
  controller_ = nullptr;
}

void SigninReauthHandler::OnGaiaReauthTypeDetermined(
    SigninReauthViewController::GaiaReauthType reauth_type) {
  if (!IsJavascriptAllowed())
    return;

  DCHECK_NE(reauth_type, SigninReauthViewController::GaiaReauthType::kUnknown);
  bool is_reauth_required =
      reauth_type != SigninReauthViewController::GaiaReauthType::kAutoApproved;
  FireWebUIListener("reauth-type-received", base::Value(is_reauth_required));
}

void SigninReauthHandler::HandleInitialize(const base::ListValue* args) {
  AllowJavascript();
}

void SigninReauthHandler::HandleConfirm(const base::ListValue* args) {
  if (controller_)
    controller_->OnReauthConfirmed(BuildConsent(args));
}

void SigninReauthHandler::HandleCancel(const base::ListValue* args) {
  if (controller_)
    controller_->OnReauthDismissed();
}

sync_pb::UserConsentTypes::AccountPasswordsConsent
SigninReauthHandler::BuildConsent(const base::ListValue* args) const {
  CHECK_EQ(2U, args->GetSize());
  base::Value::ConstListView consent_description = args->GetList()[0].GetList();
  const std::string& consent_confirmation = args->GetList()[1].GetString();

  // The strings returned by the WebUI are not free-form, they must belong into
  // a pre-determined set of strings (stored in |string_to_grd_id_map_|). As
  // this has privacy and legal implications, CHECK the integrity of the strings
  // received from the renderer process before recording the consent.
  std::vector<int> consent_description_ids;
  for (const base::Value& description : consent_description) {
    auto iter = string_to_grd_id_map_.find(description.GetString());
    CHECK(iter != string_to_grd_id_map_.end()) << "Unexpected string:\n"
                                               << description.GetString();
    consent_description_ids.push_back(iter->second);
  }

  auto iter = string_to_grd_id_map_.find(consent_confirmation);
  CHECK(iter != string_to_grd_id_map_.end()) << "Unexpected string:\n"
                                             << consent_confirmation;
  int consent_confirmation_id = iter->second;

  sync_pb::UserConsentTypes::AccountPasswordsConsent consent;
  consent.set_confirmation_grd_id(consent_confirmation_id);
  for (int id : consent_description_ids) {
    consent.add_description_grd_ids(id);
  }
  consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                         UserConsentTypes_ConsentStatus_GIVEN);

  return consent;
}
