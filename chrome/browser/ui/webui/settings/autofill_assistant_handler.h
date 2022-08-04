// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_AUTOFILL_ASSISTANT_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_AUTOFILL_ASSISTANT_HANDLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class ApcClient;

namespace settings {

// A custom WebUI handler for the personalization section in settings. It
// currently handles consent requests for Autofill Assistant.
class AutofillAssistantHandler : public SettingsPageUIHandler {
 public:
  // Constructs a personalization handler. `accepted_revoke_grd_ids` are
  // resource ids that are permitted to describe revoking consent.
  explicit AutofillAssistantHandler(
      const std::vector<int>& accepted_revoke_grd_ids);

  AutofillAssistantHandler(const AutofillAssistantHandler&) = delete;
  AutofillAssistantHandler& operator=(const AutofillAssistantHandler&) = delete;

  ~AutofillAssistantHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  // Shows the onboarding prompt for Autofill Assistant.
  void HandlePromptForConsent(const base::Value::List& args);

  // Resolves a Javascript callback (corresponding to the promise returned by
  // `PromptForConsent()`) with a boolean parameter that indicates whether the
  // prompt was accepted.
  void OnPromptResultReceived(const base::Value& callback_id, bool success);

  // Handles the request to revoke consent for Autofill Assistant. `args`
  // is expected to be the set of strings contained in the UI element shown
  // to the user.
  void HandleRevokeConsent(const base::Value::List& args);

  // Returns the `ApcClient` associated with this `WebContents`.
  ApcClient* GetApcClient();

  // A map of permitted strings from the consent revokation dialog to their
  // resource ids.
  base::flat_map<std::string, int> string_to_revoke_grd_id_map_;

  base::WeakPtrFactory<AutofillAssistantHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_AUTOFILL_ASSISTANT_HANDLER_H_
