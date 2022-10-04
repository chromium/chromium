// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/headless/headless_script_controller_impl.h"

#include "base/callback_helpers.h"
#include "base/time/default_tick_clock.h"
#include "components/autofill_assistant/browser/desktop/starter_delegate_desktop.h"
#include "components/autofill_assistant/browser/headless/client_headless.h"
#include "components/autofill_assistant/browser/public/headless_onboarding_result.h"

namespace autofill_assistant {

namespace {
HeadlessOnboardingResult MapToOnboardingResult(
    OnboardingState onboarding_state) {
  if (onboarding_state.onboarding_skipped)
    return HeadlessOnboardingResult::kSkipped;

  if (onboarding_state.onboarding_shown.has_value() &&
      !onboarding_state.onboarding_shown.value())
    return HeadlessOnboardingResult::kNotShown;

  if (!onboarding_state.onboarding_result.has_value())
    return HeadlessOnboardingResult::kUndefined;

  switch (onboarding_state.onboarding_result.value()) {
    case OnboardingResult::DISMISSED:
      return HeadlessOnboardingResult::kDismissed;
    case OnboardingResult::REJECTED:
      return HeadlessOnboardingResult::kRejected;
    case OnboardingResult::NAVIGATION:
      return HeadlessOnboardingResult::kNavigation;
    case OnboardingResult::ACCEPTED:
      return HeadlessOnboardingResult::kAccepted;
  }
}
}  // namespace

HeadlessScriptControllerImpl::HeadlessScriptControllerImpl(
    content::WebContents* web_contents,
    Starter* starter,
    std::unique_ptr<ClientHeadless> client)
    : web_contents_(web_contents),
      starter_(starter),
      client_(std::move(client)) {
  DCHECK(web_contents_ && starter_ && client_);
}

HeadlessScriptControllerImpl::~HeadlessScriptControllerImpl() = default;

void HeadlessScriptControllerImpl::StartScript(
    const base::flat_map<std::string, std::string>& script_parameters,
    base::OnceCallback<void(ScriptResult)> script_ended_callback) {
  StartScript(script_parameters, std::move(script_ended_callback),
              /*use_autofill_assistant_onboarding = */ false, base::DoNothing(),
              /*suppress_browsing_features = */ true);
}

void HeadlessScriptControllerImpl::StartScript(
    const base::flat_map<std::string, std::string>& script_parameters,
    base::OnceCallback<void(ScriptResult)> script_ended_callback,
    bool use_autofill_assistant_onboarding,
    base::OnceCallback<void()> onboarding_successful_callback,
    bool suppress_browsing_features) {
  StartScript(script_parameters, std::move(script_ended_callback),
              use_autofill_assistant_onboarding,
              std::move(onboarding_successful_callback),
              suppress_browsing_features,
              /*service = */ nullptr, /*web_controller = */ nullptr);
}
void HeadlessScriptControllerImpl::StartScript(
    const base::flat_map<std::string, std::string>& script_parameters,
    base::OnceCallback<void(ScriptResult)> script_ended_callback,
    bool use_autofill_assistant_onboarding,
    base::OnceCallback<void()> onboarding_successful_callback,
    bool suppress_browsing_features,
    std::unique_ptr<Service> service,
    std::unique_ptr<WebController> web_controller) {
  // This HeadlessScriptController is currently executing a script, so we return
  // an error.
  if (script_ended_callback_) {
    std::move(script_ended_callback).Run({false});
    return;
  }

  script_ended_callback_ = std::move(script_ended_callback);
  onboarding_successful_callback_ = std::move(onboarding_successful_callback);
  auto trigger_context = std::make_unique<TriggerContext>(
      std::make_unique<ScriptParameters>(script_parameters),
      TriggerContext::Options{
          /*experiment_ids = */ "",
          starter_->GetPlatformDependencies()->IsCustomTab(*web_contents_),
          /*onboarding_shown = */ false,
          /*is_direct_action = */ false,
          /*initial_url = */ "",
          /*is_in_chrome_triggered = */ true,
          /*is_externally_triggered = */ true,
          /*skip_autofill_assistant_onboarding = */
          !use_autofill_assistant_onboarding, suppress_browsing_features});
  starter_->CanStart(
      std::move(trigger_context),
      base::BindOnce(&HeadlessScriptControllerImpl::OnReadyToStart,
                     weak_ptr_factory_.GetWeakPtr(), std::move(service),
                     std::move(web_controller)));
}

void HeadlessScriptControllerImpl::OnReadyToStart(
    std::unique_ptr<Service> service,
    std::unique_ptr<WebController> web_controller,
    bool can_start,
    const OnboardingState& onboarding_state,
    absl::optional<GURL> url,
    std::unique_ptr<TriggerContext> trigger_context) {
  HeadlessOnboardingResult onboarding_result =
      MapToOnboardingResult(onboarding_state);
  if (!can_start || !url.has_value()) {
    std::move(script_ended_callback_).Run({false, onboarding_result});
    return;
  }

  std::move(onboarding_successful_callback_).Run();

  // TODO(b/249979875): At this point we should be sure no other Controller
  // exists on this tab. Add logic to the starter to check that's the case.
  client_->Start(
      *url, std::move(trigger_context), std::move(service),
      std::move(web_controller),
      base::BindOnce(&HeadlessScriptControllerImpl::NotifyScriptEnded,
                     weak_ptr_factory_.GetWeakPtr(), onboarding_result));
}

void HeadlessScriptControllerImpl::NotifyScriptEnded(
    HeadlessOnboardingResult onboarding_result,
    Metrics::DropOutReason reason) {
  std::move(script_ended_callback_)
      .Run({reason == Metrics::DropOutReason::SCRIPT_SHUTDOWN,
            onboarding_result});
}

}  // namespace autofill_assistant
