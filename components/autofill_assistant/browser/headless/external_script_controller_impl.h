// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_EXTERNAL_SCRIPT_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_EXTERNAL_SCRIPT_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill_assistant/browser/autofill_assistant_impl.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/controller_observer.h"
#include "components/autofill_assistant/browser/execution_delegate.h"
#include "components/autofill_assistant/browser/headless/client_headless.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/script_executor_ui_delegate.h"

namespace autofill_assistant {

class ExternalScriptControllerImpl : public ExternalScriptController {
 public:
  ExternalScriptControllerImpl(
      content::WebContents* web_contents,
      ExternalActionDelegate* action_extension_delegate);

  ExternalScriptControllerImpl(const ExternalScriptControllerImpl&) = delete;
  ExternalScriptControllerImpl& operator=(const ExternalScriptControllerImpl&) =
      delete;

  ~ExternalScriptControllerImpl() override;

  // Overrides ExternalScriptController.
  void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback) override;

  // Notifies the external caller that the script has ended. Note that the
  // external caller can decide to destroy this instance once it has been
  // notified so this method should not be called directly to avoid UAF issues.
  void NotifyScriptEnded(Metrics::DropOutReason reason);

 private:
  void OnReadyToStart(bool can_start,
                      absl::optional<GURL> url,
                      std::unique_ptr<TriggerContext> trigger_context);
  raw_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ClientHeadless> client_;

  base::OnceCallback<void(ScriptResult)> script_ended_callback_;

  base::WeakPtrFactory<ExternalScriptControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_EXTERNAL_SCRIPT_CONTROLLER_IMPL_H_
