// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_service.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_service_manager.h"

TutorialService::TutorialService() = default;
TutorialService::~TutorialService() = default;

bool TutorialService::StartTutorial(
    TutorialIdentifier id,
    ui::ElementContext context,
    TutorialBubbleFactoryRegistry* bubble_factory_registry,
    TutorialRegistry* tutorial_registry) {
  if (!bubble_factory_registry || !tutorial_registry) {
    TutorialServiceManager* tutorial_service_manager =
        TutorialServiceManager::GetInstance();

    bubble_factory_registry =
        tutorial_service_manager->bubble_factory_registry();

    tutorial_registry = tutorial_service_manager->tutorial_registry();
  }

  auto tutorial = tutorial_registry->CreateTutorial(
      id, this, bubble_factory_registry, context);
  if (!tutorial)
    return false;
  return StartTutorialImpl(std::move(tutorial));
}

bool TutorialService::StartTutorialImpl(std::unique_ptr<Tutorial> tutorial) {
  if (running_tutorial_)
    return false;

  CHECK(tutorial);

  running_tutorial_ = std::move(tutorial);
  running_tutorial_->Start();
  return true;
}

void TutorialService::SetOnCompleteTutorial(CompletedCallback callback) {
  completed_callback_ = std::move(callback);
}

void TutorialService::SetOnAbortTutorial(AbortedCallback callback) {
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

void TutorialService::SetCurrentBubble(std::unique_ptr<TutorialBubble> bubble) {
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

std::vector<TutorialIdentifier> TutorialService::GetTutorialIdentifiers()
    const {
  TutorialServiceManager* tutorial_service_manager =
      TutorialServiceManager::GetInstance();

  return tutorial_service_manager->tutorial_registry()
      ->GetTutorialIdentifiers();
}
