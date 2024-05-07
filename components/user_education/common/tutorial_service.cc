// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial_service.h"

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_registry.h"

namespace user_education {

namespace {
// How long a tutorial has to go without a bubble before we assume it's broken
// and abort it.
constexpr base::TimeDelta kBrokenTutorialTimeout = base::Seconds(15);
// How long a tutorial has to go before the first bubble is shown before we
// assume it's been broken or abandoned and abort it. This is longer than the
// above because we want to allow the user time to navigate to the surface that
// triggers the tutorial.
constexpr base::TimeDelta kTutorialNotStartedTimeout = base::Seconds(60);
}  // namespace

TutorialService::TutorialCreationParams::TutorialCreationParams(
    const TutorialDescription* description,
    ui::ElementContext context)
    : description_(description), context_(context) {}

TutorialService::TutorialService(
    TutorialRegistry* tutorial_registry,
    HelpBubbleFactoryRegistry* help_bubble_factory_registry)
    : tutorial_registry_(tutorial_registry),
      help_bubble_factory_registry_(help_bubble_factory_registry) {}

TutorialService::~TutorialService() = default;

void TutorialService::StartTutorial(TutorialIdentifier id,
                                    ui::ElementContext context,
                                    CompletedCallback completed_callback,
                                    AbortedCallback aborted_callback,
                                    RestartedCallback restarted_callback) {
  CancelTutorialIfRunning();

  // Get the description from the tutorial registry.
  const TutorialDescription* const description =
      tutorial_registry_->GetTutorialDescription(id);
  CHECK(description);

  // Construct the tutorial from the description.
  running_tutorial_ =
      Tutorial::Builder::BuildFromDescription(*description, this, context);

  // Set the external callbacks.
  completed_callback_ = std::move(completed_callback);
  aborted_callback_ = std::move(aborted_callback);
  restarted_callback_ = std::move(restarted_callback);

  // Save the params for creating the tutorial to be used when restarting.
  running_tutorial_creation_params_ =
      std::make_unique<TutorialCreationParams>(description, context);

  // Before starting the tutorial, set a timeout just in case the user never
  // actually gets to a place where they can launch the first bubble.
  broken_tutorial_timer_.Start(
      FROM_HERE, kTutorialNotStartedTimeout,
      base::BindOnce(&TutorialService::OnBrokenTutorial,
                     base::Unretained(this)));

  // Start the tutorial and mark the params used to created it for restarting.
  most_recent_tutorial_id_ = id;
  if (description->temporary_state_callback) {
    running_tutorial_->SetState(
        description->temporary_state_callback.Run(context));
  }
  running_tutorial_->Start();
}

bool TutorialService::CancelTutorialIfRunning(
    std::optional<TutorialIdentifier> id) {
  if (!running_tutorial_) {
    return false;
  }

  // If a specific tutorial was requested to be aborted, make sure that's the
  // one that is running.
  if (id.has_value() && most_recent_tutorial_id_ != id) {
    return false;
  }

  if (is_final_bubble_) {
    // The current tutorial is showing the final congratulatory bubble, so it
    // is effectively complete.
    CompleteTutorial();
    is_final_bubble_ = false;
  } else {
    running_tutorial_->Abort();
  }

  return true;
}

void TutorialService::LogIPHLinkClicked(TutorialIdentifier id,
                                        bool iph_link_was_clicked) {
  const TutorialDescription* const description =
      tutorial_registry_->GetTutorialDescription(id);
  CHECK(description);

  if (description->histograms)
    description->histograms->RecordIphLinkClicked(iph_link_was_clicked);
}

void TutorialService::LogStartedFromWhatsNewPage(TutorialIdentifier id,
                                                 bool success) {
  const TutorialDescription* const description =
      tutorial_registry_->GetTutorialDescription(id);
  CHECK(description);

  if (description->histograms)
    description->histograms->RecordStartedFromWhatsNewPage(success);
}

bool TutorialService::RestartTutorial() {
  DCHECK(running_tutorial_ && running_tutorial_creation_params_);
  base::AutoReset<bool> resetter(&is_restarting_, true);

  HideCurrentBubbleIfShowing();

  running_tutorial_ = Tutorial::Builder::BuildFromDescription(
      *running_tutorial_creation_params_->description_, this,
      running_tutorial_creation_params_->context_);
  if (!running_tutorial_) {
    ResetRunningTutorial();
    return false;
  }

  if (running_tutorial_creation_params_->description_
          ->temporary_state_callback) {
    running_tutorial_->SetState(
        running_tutorial_creation_params_->description_
            ->temporary_state_callback.Run(
                running_tutorial_creation_params_->context_));
  }

  // Note: if we restart the tutorial, we won't record whether the user pressed
  // the pane focus key to focus the help bubble until the user actually decides
  // they're finished, but we also won't reset the count, so at the end we can
  // record the total.
  // TODO(dfried): decide if this is actually the correct behavior.
  running_tutorial_was_restarted_ = true;
  running_tutorial_->Start();

  restarted_callback_.Run();

  return true;
}

void TutorialService::AbortTutorial(std::optional<int> abort_step) {
  // For various reasons, we could get called here while e.g. tearing down the
  // interaction sequence. We only want to actually run AbortTutorial() or
  // CompleteTutorial() exactly once, so we won't continue if the tutorial has
  // already been disposed. We also only want to abort the tutorial if we are
  // not in the process of restarting. When calling reset on the help bubble,
  // or when resetting the tutorial, the interaction sequence or callbacks could
  // call the abort.
  if (!running_tutorial_ || is_restarting_)
    return;

  // If the tutorial had been restarted and then aborted, The tutorial should be
  // considered completed.
  if (running_tutorial_was_restarted_) {
    CompleteTutorial();
    return;
  }

  if (running_tutorial_creation_params_->description_->histograms) {
    if (abort_step.has_value()) {
      running_tutorial_creation_params_->description_->histograms
          ->RecordAbortStep(abort_step.value());
    }
    running_tutorial_creation_params_->description_->histograms->RecordComplete(
        false);
  }

  UMA_HISTOGRAM_BOOLEAN("Tutorial.Completion", false);

  // Reset the tutorial and call the external abort callback.
  ResetRunningTutorial();

  if (aborted_callback_) {
    std::move(aborted_callback_).Run();
  }
}

void TutorialService::OnNonFinalBubbleClosed(HelpBubble* bubble,
                                             HelpBubble::CloseReason) {
  if (bubble != currently_displayed_bubble_.get()) {
    return;
  }

  bubble_closed_subscription_ = base::CallbackListSubscription();
  currently_displayed_bubble_.reset();

  broken_tutorial_timer_.Start(
      FROM_HERE, kBrokenTutorialTimeout,
      base::BindOnce(&TutorialService::OnBrokenTutorial,
                     base::Unretained(this)));
}

void TutorialService::CompleteTutorial() {
  DCHECK(running_tutorial_);

  // Log the completion metric based on if the tutorial was restarted or not.
  if (running_tutorial_creation_params_->description_->histograms)
    running_tutorial_creation_params_->description_->histograms->RecordComplete(
        true);
  UMA_HISTOGRAM_BOOLEAN("Tutorial.Completion", true);

  ResetRunningTutorial();

  std::move(completed_callback_).Run();
}

void TutorialService::SetCurrentBubble(std::unique_ptr<HelpBubble> bubble,
                                       bool is_last_step) {
  DCHECK(running_tutorial_);
  currently_displayed_bubble_ = std::move(bubble);
  broken_tutorial_timer_.Stop();
  if (is_last_step) {
    is_final_bubble_ = true;
    bubble_closed_subscription_ =
        currently_displayed_bubble_->AddOnCloseCallback(base::BindOnce(
            [](TutorialService* service, HelpBubble*, HelpBubble::CloseReason) {
              service->CompleteTutorial();
            },
            base::Unretained(this)));
  } else {
    is_final_bubble_ = false;
    bubble_closed_subscription_ =
        currently_displayed_bubble_->AddOnCloseCallback(base::BindOnce(
            &TutorialService::OnNonFinalBubbleClosed, base::Unretained(this)));
  }
}

void TutorialService::HideCurrentBubbleIfShowing() {
  if (!currently_displayed_bubble_)
    return;
  bubble_closed_subscription_ = base::CallbackListSubscription();
  currently_displayed_bubble_.reset();
}

bool TutorialService::IsRunningTutorial(
    std::optional<TutorialIdentifier> id) const {
  if (!running_tutorial_) {
    return false;
  }
  return !id.has_value() || id.value() == most_recent_tutorial_id_;
}

void TutorialService::ResetRunningTutorial() {
  DCHECK(running_tutorial_);
  broken_tutorial_timer_.Stop();
  running_tutorial_.reset();
  running_tutorial_creation_params_.reset();
  running_tutorial_was_restarted_ = false;
  HideCurrentBubbleIfShowing();
}

void TutorialService::OnBrokenTutorial() {
  if (running_tutorial_ && !currently_displayed_bubble_) {
    running_tutorial_->Abort();
  }
}

}  // namespace user_education
