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
                                    ui::ElementContext context) {
  return StartTutorialImpl(tutorial_registry_->CreateTutorial(
      id, this, help_bubble_factory_registry_, context));
}

bool TutorialService::StartTutorialImpl(std::unique_ptr<Tutorial> tutorial) {
  if (running_tutorial_)
    return false;

  CHECK(tutorial);

  running_tutorial_ = std::move(tutorial);
  running_tutorial_->Start();
  return true;
}

void TutorialService::SetOnCompleteTutorialForTesting(
    CompletedCallback callback) {
  completed_callback_ = std::move(callback);
}

void TutorialService::SetOnAbortTutorialForTesting(AbortedCallback callback) {
  aborted_callback_ = std::move(callback);
}

void TutorialService::AbortTutorial() {
  running_tutorial_.reset();
  currently_displayed_bubble_.reset();
  if (aborted_callback_)
    aborted_callback_.Run();
}

void TutorialService::CompleteTutorial() {
  running_tutorial_.reset();
  if (completed_callback_)
    completed_callback_.Run();

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
