// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_management_flow_controller.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/profiles/profile_management_step_controller.h"
#include "chrome/browser/ui/views/profiles/profile_management_types.h"
#include "chrome/browser/ui/views/profiles/profile_picker_web_contents_host.h"

namespace {

// LINT.IfChange(GetStepHistogramSuffix)
std::string_view GetStepHistogramSuffix(
    ProfileManagementFlowController::Step step) {
  switch (step) {
    case ProfileManagementFlowController::Step::kUnknown:
      NOTREACHED();
    case ProfileManagementFlowController::Step::kProfilePicker:
      return ".PickerMainApp";
    case ProfileManagementFlowController::Step::kAccountSelection:
      return ".SigninFlow";
    case ProfileManagementFlowController::Step::kFinishSamlSignin:
      return ".FinishSamlFlow";
    case ProfileManagementFlowController::Step::kReauth:
      return ".Reauth";
    case ProfileManagementFlowController::Step::kPostSignInFlow:
      return ".PostSignInSteps";
    case ProfileManagementFlowController::Step::kIntro:
      return ".FREIntro";
    case ProfileManagementFlowController::Step::kDefaultBrowser:
      return ".DefaultBrowser";
    case ProfileManagementFlowController::Step::kSearchEngineChoice:
      return ".SearchEngineChoice";
    case ProfileManagementFlowController::Step::kFinishFlow:
      return ".FinishFlow";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/profile/histograms.xml:StepName)

}  // namespace

ProfileManagementFlowController::ProfileManagementFlowController(
    ProfilePickerWebContentsHost* host,
    ClearHostClosure clear_host_callback,
    std::string_view flow_type_string)
    : host_(host),
      clear_host_callback_(std::move(clear_host_callback)),
      flow_tracker_(flow_type_string) {
  DCHECK(clear_host_callback_.value());
}

ProfileManagementFlowController::~ProfileManagementFlowController() {
  flow_tracker_.ExitedFlow();
}

void ProfileManagementFlowController::SwitchToStep(
    Step step,
    bool reset_state,
    StepSwitchFinishedCallback step_switch_finished_callback,
    base::OnceClosure pop_step_callback) {
  Step previous_step = flow_tracker_.tracked_step();
  if (previous_step != Step::kUnknown) {
    flow_tracker_.ExitedCurrentStep();
  }
  flow_tracker_.EnteredNewStep(step);

  StepSwitchFinishedCallback internal_step_switch_finished_callback =
      StepSwitchFinishedCallback(
          base::BindOnce(&FlowTracker::FinishedStepSwitch,
                         base::Unretained(&flow_tracker_), step));

  std::vector<StepSwitchFinishedCallback> callbacks;
  callbacks.push_back(std::move(internal_step_switch_finished_callback));
  callbacks.push_back(std::move(step_switch_finished_callback));
  StepSwitchFinishedCallback combined_step_switch_callbacks =
      CombineCallbacks<StepSwitchFinishedCallback, bool>(std::move(callbacks));

  auto* new_step_controller = initialized_steps_.at(step).get();
  DCHECK(new_step_controller);
  new_step_controller->set_pop_step_callback(std::move(pop_step_callback));
  new_step_controller->Show(std::move(combined_step_switch_callbacks),
                            reset_state);

  if (initialized_steps_.contains(previous_step)) {
    initialized_steps_.at(previous_step)->OnHidden();
  }
}

void ProfileManagementFlowController::OnNavigateBackRequested() {
  DCHECK(initialized_steps_.contains(flow_tracker_.tracked_step()));
  initialized_steps_.at(flow_tracker_.tracked_step())
      ->OnNavigateBackRequested();
}

void ProfileManagementFlowController::OnReloadRequested() {
  DCHECK(initialized_steps_.contains(flow_tracker_.tracked_step()));
  initialized_steps_.at(flow_tracker_.tracked_step())->OnReloadRequested();
}

std::u16string
ProfileManagementFlowController::GetFallbackAccessibleWindowTitle() const {
  return std::u16string();
}

void ProfileManagementFlowController::RegisterStep(
    Step step,
    std::unique_ptr<ProfileManagementStepController> step_controller) {
  initialized_steps_[step] = std::move(step_controller);
}

void ProfileManagementFlowController::UnregisterStep(Step step) {
  CHECK_NE(step, flow_tracker_.tracked_step());
  initialized_steps_.erase(step);
}

bool ProfileManagementFlowController::IsStepInitialized(Step step) const {
  return initialized_steps_.contains(step) && initialized_steps_.at(step);
}

bool ProfileManagementFlowController::HasFlowExited() const {
  return clear_host_callback_.value().is_null();
}

void ProfileManagementFlowController::ExitFlow() {
  CHECK(!HasFlowExited());
  std::move(clear_host_callback_.value()).Run();
}

bool ProfileManagementFlowController::PreFinishWithBrowser() {
  return false;
}

ProfileManagementFlowController::Step
ProfileManagementFlowController::current_step() const {
  return flow_tracker_.tracked_step();
}

void ProfileManagementFlowController::FinishFlowAndRunInBrowser(
    Profile* profile,
    PostHostClearedCallback post_host_cleared_callback) {
  DCHECK(clear_host_callback_.value());  // The host shouldn't be cleared yet.

  // TODO(crbug.com/40246333): Handle the return value and don't open a browser
  // if it is already going to be opened.
  PreFinishWithBrowser();

  base::OnceCallback<void(Browser*)> post_browser_open_callback;
  // `clear_host_callback_` and `post_host_cleared_callback` may be run after
  // the `ProfileManagementFlowController` is deleted.
  if (post_host_cleared_callback->is_null()) {
    post_browser_open_callback =
        base::IgnoreArgs<Browser*>(std::move(clear_host_callback_.value()));
  } else {
    post_browser_open_callback =
        base::BindOnce(
            [](base::OnceClosure clear_host_closure, Browser* browser) {
              std::move(clear_host_closure).Run();
              return browser;
            },
            std::move(clear_host_callback_.value()))
            .Then(std::move(post_host_cleared_callback.value()));
  }

  bool open_command_line_urls = ProfilePicker::GetOpenCommandLineUrlsInNextProfileOpened();
  ProfilePicker::SetOpenCommandLineUrlsInNextProfileOpened(false);

  // Start by opening the browser window, to ensure that we have another
  // KeepAlive for `profile` by the time we clear the flow and its host.
  // TODO(crbug.com/40242414): Make sure we do something or log an error if
  // opening a browser window was not possible.
  profiles::OpenBrowserWindowForProfile(
      std::move(post_browser_open_callback),
      /*always_create=*/false,   // Don't create a window if one already exists.
      /*is_new_profile=*/false,  // Don't create a first run window.
      open_command_line_urls, profile);
}

base::OnceClosure
ProfileManagementFlowController::CreateSwitchToStepPopCallback(Step step) {
  return base::BindOnce(
      &ProfileManagementFlowController::SwitchToStep,
      // Binding as Unretained as `this` outlives the step
      // controllers.
      base::Unretained(this), step,
      /*reset_state=*/false,
      /*step_switch_finished_callback=*/StepSwitchFinishedCallback(),
      /*pop_step_callback=*/base::OnceClosure());
}

void ProfileManagementFlowController::CreateSignedOutFlowWebContents(
    Profile* profile) {
  signed_out_flow_web_contents_ =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
}

content::WebContents*
ProfileManagementFlowController::GetSignedOutFlowWebContents() const {
  return signed_out_flow_web_contents_.get();
}

void ProfileManagementFlowController::Reset(
    StepSwitchFinishedCallback callback) {
  Step previous_step = flow_tracker_.tracked_step();

  // Activate the initial step.
  SwitchToStep(Step::kProfilePicker, /*reset_state=*/true,
               /*step_switch_finished_callback=*/std::move(callback));
  // Unregister the previous active step.
  UnregisterStep(previous_step);
}

ProfileManagementFlowController::FlowTracker::FlowTracker(
    std::string_view flow_type_string)
    : flow_type_string_(flow_type_string) {}

void ProfileManagementFlowController::FlowTracker::EnteredNewStep(Step step) {
  CHECK_NE(Step::kUnknown, step);
  CHECK_NE(tracked_step_, step);
  tracked_step_ = step;

  base::UmaHistogramEnumeration(
      base::StrCat({"ProfilePicker.", flow_type_string_, ".StepStart"}), step);

  step_start_elapsed_timer_.emplace();
}

void ProfileManagementFlowController::FlowTracker::FinishedStepSwitch(
    Step step,
    bool success) {
  if (tracked_step_ != step) {
    NOTREACHED(base::NotFatalUntil::M143)
        << "Step switch callback should run while the step is still the "
           "current step being tracked.";
    return;
  }

  if (!success) {
    base::UmaHistogramEnumeration(
        base::StrCat({"ProfilePicker.", flow_type_string_, ".StepSkipped"}),
        step);
    return;
  }

  step_shown_elapsed_timer_.emplace();
  base::UmaHistogramEnumeration(
      base::StrCat({"ProfilePicker.", flow_type_string_, ".StepShown"}), step);
}

void ProfileManagementFlowController::FlowTracker::ExitedCurrentStep() {
  CHECK(step_start_elapsed_timer_.has_value());

  base::UmaHistogramEnumeration(
      base::StrCat({"ProfilePicker.", flow_type_string_, ".StepEnd"}),
      tracked_step_);

  const std::string step_total_duration_base_histogram =
      base::StrCat({"ProfilePicker.", flow_type_string_, ".StepTotalDuration"});
  base::TimeDelta step_start_exit_elapsed_time =
      step_start_elapsed_timer_->Elapsed();
  base::UmaHistogramMediumTimes(step_total_duration_base_histogram,
                                step_start_exit_elapsed_time);
  base::UmaHistogramMediumTimes(
      base::StrCat({step_total_duration_base_histogram,
                    GetStepHistogramSuffix(tracked_step_)}),
      step_start_exit_elapsed_time);
  step_start_elapsed_timer_.reset();

  if (step_shown_elapsed_timer_) {
    const std::string step_shown_duration_base_histogram = base::StrCat(
        {"ProfilePicker.", flow_type_string_, ".StepShownDuration"});
    base::TimeDelta step_shown_exit_elapsed_time =
        step_shown_elapsed_timer_->Elapsed();
    base::UmaHistogramMediumTimes(step_shown_duration_base_histogram,
                                  step_shown_exit_elapsed_time);
    base::UmaHistogramMediumTimes(
        base::StrCat({step_shown_duration_base_histogram,
                      GetStepHistogramSuffix(tracked_step_)}),
        step_shown_exit_elapsed_time);
    step_shown_elapsed_timer_.reset();
  }
}

void ProfileManagementFlowController::FlowTracker::ExitedFlow() {
  // Records the last active step metrics if not already done.
  if (step_start_elapsed_timer_.has_value()) {
    ExitedCurrentStep();
  }

  base::UmaHistogramMediumTimes(
      base::StrCat(
          {"ProfilePicker." + flow_type_string_ + ".FlowTotalDuration"}),
      flow_elapsed_timer_.Elapsed());

  base::UmaHistogramEnumeration(
      base::StrCat({"ProfilePicker." + flow_type_string_ + ".FlowEndedAtStep"}),
      tracked_step_);
}

std::string_view GetStepHistogramSuffixForTesting(
    ProfileManagementFlowController::Step step) {
  CHECK_IS_TEST();
  return GetStepHistogramSuffix(step);
}
