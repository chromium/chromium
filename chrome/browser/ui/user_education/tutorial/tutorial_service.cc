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

TutorialService::TutorialService(
    std::unique_ptr<TutorialRegistry> tutorial_registry)
    : tutorial_registry_(std::move(tutorial_registry)) {}

TutorialService::~TutorialService() = default;

bool TutorialService::StartTutorial(TutorialIdentifier id,
                                    ui::ElementContext context) {
  TutorialBubbleFactoryRegistry* factory_registry =
      TutorialServiceManager::GetInstance()->bubble_factory_registry();

  return StartTutorialImpl(
      tutorial_registry_->CreateTutorial(id, this, factory_registry, context));
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
}

void TutorialService::CompleteTutorial() {
  // TODO (dpenning): Add in hooks to listen for complete
  running_tutorial_.reset();
}

void TutorialService::SetCurrentBubble(std::unique_ptr<TutorialBubble> bubble) {
  HideCurrentBubbleIfShowing();
  currently_displayed_bubble_ = std::move(bubble);
}

void TutorialService::HideCurrentBubbleIfShowing() {
  if (currently_displayed_bubble_) {
    currently_displayed_bubble_.release();
  }
}

bool TutorialService::IsRunningTutorial() const {
  return running_tutorial_ != nullptr;
}

std::vector<TutorialIdentifier> TutorialService::GetTutorialIdentifiers()
    const {
  return tutorial_registry_->GetTutorialIdentifiers();
}
