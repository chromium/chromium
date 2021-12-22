// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"

#include <memory>
#include <vector>

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_description.h"

TutorialRegistry::TutorialRegistry() = default;
TutorialRegistry::~TutorialRegistry() = default;

std::unique_ptr<Tutorial> TutorialRegistry::CreateTutorial(
    TutorialIdentifier id,
    TutorialService* tutorial_service,
    TutorialBubbleFactoryRegistry* bubble_factory_registry,
    ui::ElementContext context) {
  DCHECK(tutorial_registry_.size() > 0);
  auto pair = tutorial_registry_.find(id);
  if (pair == tutorial_registry_.end())
    return nullptr;
  return Tutorial::Builder::BuildFromDescription(
      pair->second, tutorial_service, bubble_factory_registry, context);
}

const std::vector<TutorialIdentifier>
TutorialRegistry::GetTutorialIdentifiers() {
  DCHECK(tutorial_registry_.size() > 0);
  std::vector<TutorialIdentifier> id_strings;
  std::transform(tutorial_registry_.begin(), tutorial_registry_.end(),
                 std::back_inserter(id_strings),
                 [](const auto& pair) { return pair.first; });
  return id_strings;
}

void TutorialRegistry::AddTutorial(TutorialIdentifier id,
                                   TutorialDescription description) {
  tutorial_registry_.insert(std::make_pair(id, description));
}
