// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/autofill_assistant_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/autofill_assistant/password_change/apc_client.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

AutofillAssistantHandler::AutofillAssistantHandler(
    const std::vector<int>& accepted_revoke_grd_ids) {
  for (int id : accepted_revoke_grd_ids) {
    string_to_revoke_grd_id_map_[l10n_util::GetStringUTF8(id)] = id;
  }
}

AutofillAssistantHandler::~AutofillAssistantHandler() = default;

void AutofillAssistantHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "PromptForAutofillAssistantConsent",
      base::BindRepeating(&AutofillAssistantHandler::HandlePromptForConsent,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "RevokeAutofillAssistantConsent",
      base::BindRepeating(&AutofillAssistantHandler::HandleRevokeConsent,
                          base::Unretained(this)));
}

void AutofillAssistantHandler::OnJavascriptAllowed() {}

void AutofillAssistantHandler::OnJavascriptDisallowed() {
  // Ensures that there are no attempts to resolve a callback after Javascript
  // has been disabled.
  weak_ptr_factory_.InvalidateWeakPtrs();
}

void AutofillAssistantHandler::HandlePromptForConsent(
    const base::Value::List& args) {
  CHECK(!args.empty());
  AllowJavascript();
  base::Value callback_id = args.front().Clone();
  GetApcClient()->PromptForConsent(
      base::BindOnce(&AutofillAssistantHandler::OnPromptResultReceived,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback_id)));
}

void AutofillAssistantHandler::OnPromptResultReceived(
    const base::Value& callback_id,
    bool success) {
  ResolveJavascriptCallback(callback_id, base::Value(success));
}

void AutofillAssistantHandler::HandleRevokeConsent(
    const base::Value::List& args) {
  std::vector<int> description_grd_ids;

  for (const base::Value& element : args) {
    CHECK(element.is_string());

    auto grd_id = string_to_revoke_grd_id_map_.find(element.GetString());
    CHECK(grd_id != string_to_revoke_grd_id_map_.end());
    description_grd_ids.push_back(grd_id->second);
  }
  GetApcClient()->RevokeConsent(description_grd_ids);
}

ApcClient* AutofillAssistantHandler::GetApcClient() {
  return ApcClient::GetOrCreateForWebContents(web_ui()->GetWebContents());
}

}  // namespace settings
