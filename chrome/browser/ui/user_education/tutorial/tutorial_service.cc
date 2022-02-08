// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/user_education/help_bubble.h"
#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"

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
  if (running_tutorial_)
    return false;

  running_tutorial_ = tutorial_registry_->CreateTutorial(id, this, context);
  CHECK(running_tutorial_);

  completed_callback_ = std::move(completed_callback);
  aborted_callback_ = std::move(aborted_callback);
  running_tutorial_->Start();

  return true;
}

void TutorialService::AbortTutorial() {
  // For various reasons, we could get called here while e.g. tearing down the
  // interaction sequence. We only want to actually run AbortTutorial() or
  // CompleteTutorial() exactly once, so we won't continue if the tutorial has
  // already been disposed.
  if (!running_tutorial_)
    return;

  running_tutorial_.reset();
  currently_displayed_bubble_.reset();
  std::move(aborted_callback_).Run();
}

void TutorialService::CompleteTutorial() {
  // We should never call this after AbortTutorial() or call it twice, so this
  // is a useful sanity check.
  DCHECK(running_tutorial_);
  running_tutorial_.reset();
  std::move(completed_callback_).Run();

  // TODO (dpenning): decide what to do with the currently displayed bubble, we
  // want it to stick around for a while, but we also want to cleanup the
  // tutorial at some point.
}

void TutorialService::SetCurrentBubble(std::unique_ptr<HelpBubble> bubble) {
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
