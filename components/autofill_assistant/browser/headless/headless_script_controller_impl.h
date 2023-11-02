// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_HEADLESS_SCRIPT_CONTROLLER_IMPL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_HEADLESS_SCRIPT_CONTROLLER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill_assistant/browser/metrics.h"
#include "components/autofill_assistant/browser/onboarding_result.h"
#include "components/autofill_assistant/browser/public/headless_onboarding_result.h"
#include "components/autofill_assistant/browser/public/headless_script_controller.h"
#include "components/autofill_assistant/browser/starter.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill_assistant {

class ClientHeadless;
class Service;
class Starter;
class WebController;
class TriggerContext;
class HeadlessScriptControllerImplTest;

class HeadlessScriptControllerImpl : public HeadlessScriptController {
 public:
  // `starter` must outlive this instance.
  HeadlessScriptControllerImpl(content::WebContents* web_contents,
                               Starter* starter,
                               std::unique_ptr<ClientHeadless> client);

  HeadlessScriptControllerImpl(const HeadlessScriptControllerImpl&) = delete;
  HeadlessScriptControllerImpl& operator=(const HeadlessScriptControllerImpl&) =
      delete;

  ~HeadlessScriptControllerImpl() override;

  // Overrides HeadlessScriptController.
  void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback) override;
  void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback,
      bool use_autofill_assistant_onboarding,
      base::OnceCallback<void()> onboarding_successful_callback,
      bool suppress_browsing_features) override;

 private:
  friend HeadlessScriptControllerImplTest;

  void StartScript(
      const base::flat_map<std::string, std::string>& script_parameters,
      base::OnceCallback<void(ScriptResult)> script_ended_callback,
      bool use_autofill_assistant_onboarding,
      base::OnceCallback<void()> onboarding_successful_callback,
      bool suppress_browsing_features,
      std::unique_ptr<Service> service,
      std::unique_ptr<WebController> web_controller);

  void OnReadyToStart(std::unique_ptr<Service> service,
                      std::unique_ptr<WebController> web_controller,
                      bool can_start,
                      const OnboardingState& onboarding_state,
                      absl::optional<GURL> url,
                      std::unique_ptr<TriggerContext> trigger_context);

  // Notifies the external caller that the script has ended. Note that the
  // external caller can decide to destroy this instance once it has been
  // notified so this method should not be called directly to avoid UAF issues.
  void NotifyScriptEnded(HeadlessOnboardingResult onboarding_result,
                         Metrics::DropOutReason reason);

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<Starter> starter_;
  std::unique_ptr<ClientHeadless> client_;

  base::OnceCallback<void(ScriptResult)> script_ended_callback_;

  base::OnceCallback<void()> onboarding_successful_callback_;

  base::WeakPtrFactory<HeadlessScriptControllerImpl> weak_ptr_factory_{this};
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_HEADLESS_HEADLESS_SCRIPT_CONTROLLER_IMPL_H_
