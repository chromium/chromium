// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "chrome/browser/ui/user_education/help_bubble.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"

TutorialService::TutorialCreationParams::TutorialCreationParams(
    TutorialDescription* description,
    ui::ElementContext context)
    : description_(description), context_(context) {}

TutorialService::TutorialService(
    TutorialRegistry* tutorial_registry,
    HelpBubbleFactoryRegistry* help_bubble_factory_registry)
    : tutorial_registry_(tutorial_registry),
      help_bubble_factory_registry_(help_bubble_factory_registry) {}
TutorialService::~TutorialService() = default;

bool TutorialService::StartTutorial(TutorialIdentifier id,
                                    ui::ElementContext context,
                                    CompletedCallback completed_callback,
                                    AbortedCallback aborted_callback) {
  // Overriding an existing running tutorial is not supported. In this case
  // return false to the caller.
  if (running_tutorial_)
    return false;

  // Get the description from the tutorial registry.
  TutorialDescription* description =
      tutorial_registry_->GetTutorialDescription(id);
  CHECK(description);

  // Construct the tutorial from the dsecription.
  running_tutorial_ =
      Tutorial::Builder::BuildFromDescription(*description, this, context);

  // Set the external callbacks.
  completed_callback_ = std::move(completed_callback);
  aborted_callback_ = std::move(aborted_callback);

  // Save the params for creating the tutorial to be used when restarting.
  running_tutorial_creation_params_ =
      std::make_unique<TutorialCreationParams>(description, context);

  // Start the tutorial and mark the params used to created it for restarting.
  running_tutorial_->Start();

  return true;
}

bool TutorialService::RestartTutorial() {
  DCHECK(running_tutorial_ && running_tutorial_creation_params_);
  base::AutoReset<bool> resetter(&is_restarting_, true);

  currently_displayed_bubble_.reset();

  running_tutorial_ = Tutorial::Builder::BuildFromDescription(
      *running_tutorial_creation_params_->description_, this,
      running_tutorial_creation_params_->context_);
  if (!running_tutorial_) {
    ResetRunningTutorial();
    return false;
  }

  running_tutorial_was_restarted_ = true;
  running_tutorial_->Start();

  return true;
}

void TutorialService::AbortTutorial() {
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
  if (running_tutorial_was_restarted_)
    return CompleteTutorial();

  // TODO:(crbug.com/1295165) provide step number information from the
  // interaction sequence into the abort callback.

  // Log the failure of completion for the tutorial.
  if (running_tutorial_creation_params_->description_->histograms)
    running_tutorial_creation_params_->description_->histograms->RecordComplete(
        false);
  UMA_HISTOGRAM_BOOLEAN("Tutorial.Completion", false);

  // Reset the tutorial and call the external abort callback.
  ResetRunningTutorial();

  if (aborted_callback_) {
    std::move(aborted_callback_).Run();
  }
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

void TutorialService::SetCurrentBubble(std::unique_ptr<HelpBubble> bubble) {
  DCHECK(running_tutorial_);
  currently_displayed_bubble_ = std::move(bubble);
}

void TutorialService::HideCurrentBubbleIfShowing() {
  if (currently_displayed_bubble_) {
    currently_displayed_bubble_.reset();
  }
}

bool TutorialService::IsRunningTutorial() const {
  return running_tutorial_ != nullptr;
}

void TutorialService::ResetRunningTutorial() {
  DCHECK(running_tutorial_);
  running_tutorial_.reset();
  running_tutorial_creation_params_.reset();
  running_tutorial_was_restarted_ = false;
  currently_displayed_bubble_.reset();
}
