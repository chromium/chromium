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

bool TutorialService::StartTutorial(TutorialIdentifier id,
                                    ui::ElementContext context) {
  TutorialServiceManager* tutorial_service_manager =
      TutorialServiceManager::GetInstance();

  TutorialBubbleFactoryRegistry* factory_registry =
      tutorial_service_manager->bubble_factory_registry();

  TutorialRegistry* tutorial_registry =
      tutorial_service_manager->tutorial_registry();

  return StartTutorialImpl(
      tutorial_registry->CreateTutorial(id, this, factory_registry, context));
}

bool TutorialService::StartTutorialImpl(std::unique_ptr<Tutorial> tutorial) {
  if (running_tutorial_)
    return false;

  running_tutorial_ = std::move(tutorial);
  running_tutorial_->Start();
  return true;
}

void TutorialService::AbortTutorial() {
  // TODO (dpenning): Add in hooks to listen for abort
  running_tutorial_.reset();
  currently_displayed_bubble_.reset();
}

void TutorialService::CompleteTutorial() {
  // TODO (dpenning): Add in hooks to listen for complete
  running_tutorial_.reset();

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
