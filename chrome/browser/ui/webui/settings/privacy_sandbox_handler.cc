// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/privacy_sandbox_handler.h"

#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings.h"
#include "chrome/browser/privacy_sandbox/privacy_sandbox_settings_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace settings {

void PrivacySandboxHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getFlocId", base::BindRepeating(&PrivacySandboxHandler::HandleGetFlocId,
                                       base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "resetFlocId",
      base::BindRepeating(&PrivacySandboxHandler::HandleResetFlocId,
                          base::Unretained(this)));
}

void PrivacySandboxHandler::HandleGetFlocId(const base::ListValue* args) {
  AllowJavascript();

  CHECK_EQ(1U, args->GetSize());
  const base::Value* callback_id;
  CHECK(args->Get(0, &callback_id));

  auto* privacy_sandbox_settings = PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_settings);

  ResolveJavascriptCallback(
      *callback_id,
      base::Value(privacy_sandbox_settings->GetFlocIdForDisplay()));
}

void PrivacySandboxHandler::HandleResetFlocId(const base::ListValue* args) {
  CHECK_EQ(0U, args->GetList().size());
  AllowJavascript();

  auto* privacy_sandbox_settings = PrivacySandboxSettingsFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));
  DCHECK(privacy_sandbox_settings);

  privacy_sandbox_settings->SetFlocDataAccessibleFromNow(
      /*reset_calculate_timer=*/true);

  // The identifier will have been immediately invalidated in response to
  // the clearing action, so synchronously retrieving the FLoC ID will retrieve
  // the appropriate invalid ID string.
  // TODO(crbug.com/1207891): Have this handler listen to an event directly
  // from the FLoC provider, rather than inferring behavior.
  FireWebUIListener(
      "floc-id-changed",
      base::Value(privacy_sandbox_settings->GetFlocIdForDisplay()));
}

}  // namespace settings
