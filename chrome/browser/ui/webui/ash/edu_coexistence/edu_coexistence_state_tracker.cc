// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/edu_coexistence/edu_coexistence_state_tracker.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "content/public/browser/web_ui.h"

namespace ash {

namespace {

constexpr char kEduCoexistenceV2InSessionFlowResult[] =
    "AccountManager.EduCoexistenceV2.InSessionFlowResult";

constexpr char kEduCoexistenceV2OnboardingFlowResult[] =
    "AccountManager.EduCoexistenceV2.OnboardingFlowResult";

}  // namespace

EduCoexistenceStateTracker::FlowState::FlowState() = default;

EduCoexistenceStateTracker::FlowState::~FlowState() = default;

// static
EduCoexistenceStateTracker* EduCoexistenceStateTracker::Get() {
  static base::NoDestructor<EduCoexistenceStateTracker> instance;
  return instance.get();
}

void EduCoexistenceStateTracker::SetEduConsentCallback(
    const content::WebUI* web_ui,
    const std::string& account_email,
    base::OnceCallback<void(bool)> consent_logged_callback) {
  DCHECK(base::Contains(state_tracker_, web_ui));
  FlowState& state = state_tracker_[web_ui];

  if (state.received_consent) {
    DCHECK_EQ(state.email, account_email);
    std::move(consent_logged_callback).Run(/* success */ true);
    return;
  }

  DCHECK(state.email.empty());
  state.email = account_email;
  state.consent_logged_callback = std::move(consent_logged_callback);
}

void EduCoexistenceStateTracker::OnDialogClosed(const content::WebUI* web_ui) {
  if (!base::Contains(state_tracker_, web_ui))
    return;

  FlowState& state = state_tracker_[web_ui];

  // If the consent_logged_callback is still present when the dialog is closed,
  // then EduCoexistenceLoginHandler has not received a parental consent logged
  // signal. Notify |EduCoexistenceChildSigninHelper| that we weren't able to
  // successfully log the parental consent so that it can clean up after itself.
  if (state.consent_logged_callback)
    std::move(state.consent_logged_callback).Run(/* success */ false);

  const std::string& histogram_name =
      state.is_onboarding ? kEduCoexistenceV2OnboardingFlowResult
                          : kEduCoexistenceV2InSessionFlowResult;
  base::UmaHistogramEnumeration(histogram_name, state.flow_result,
                                FlowResult::kNumStates);

  state_tracker_.erase(web_ui);
}

void EduCoexistenceStateTracker::OnDialogCreated(const content::WebUI* web_ui,
                                                 bool is_onboarding) {
  DCHECK(!base::Contains(state_tracker_, web_ui));

  FlowState& state = state_tracker_[web_ui];

  state.received_consent = false;
  state.flow_result = FlowResult::kLaunched;
  state.is_onboarding = is_onboarding;
}

void EduCoexistenceStateTracker::OnConsentLogged(
    const content::WebUI* web_ui,
    const std::string& account_email) {
  DCHECK(base::Contains(state_tracker_, web_ui));

  // Update the webui state that consent was logged.
  OnWebUiStateChanged(web_ui, FlowResult::kConsentLogged);

  if (state_tracker_[web_ui].consent_logged_callback) {
    DCHECK_EQ(state_tracker_[web_ui].email, account_email);
    std::move(state_tracker_[web_ui].consent_logged_callback)
        .Run(/* success */ true);
    return;
  }

  state_tracker_[web_ui].received_consent = true;
  state_tracker_[web_ui].email = account_email;
}

void EduCoexistenceStateTracker::OnWebUiStateChanged(
    const content::WebUI* web_ui,
    FlowResult result) {
  DCHECK(base::Contains(state_tracker_, web_ui));
  state_tracker_[web_ui].flow_result = result;
}

const EduCoexistenceStateTracker::FlowState*
EduCoexistenceStateTracker::GetInfoForWebUIForTest(
    const content::WebUI* web_ui) const {
  if (!base::Contains(state_tracker_, web_ui))
    return nullptr;
  return &state_tracker_.at(web_ui);
}

std::string EduCoexistenceStateTracker::GetInSessionHistogramNameForTest()
    const {
  return kEduCoexistenceV2InSessionFlowResult;
}

EduCoexistenceStateTracker::EduCoexistenceStateTracker() = default;

EduCoexistenceStateTracker::~EduCoexistenceStateTracker() = default;

}  // namespace ash
