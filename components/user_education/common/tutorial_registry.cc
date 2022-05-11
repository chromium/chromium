// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial_registry.h"

#include <memory>
#include <vector>

#include "base/containers/contains.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"

namespace user_education {

TutorialRegistry::TutorialRegistry() = default;
TutorialRegistry::~TutorialRegistry() = default;

bool TutorialRegistry::IsTutorialRegistered(TutorialIdentifier id) const {
  return base::Contains(tutorial_registry_, id);
}

TutorialDescription* TutorialRegistry::GetTutorialDescription(
    TutorialIdentifier id) {
  DCHECK(tutorial_registry_.size() > 0);
  auto pair = tutorial_registry_.find(id);
  if (pair == tutorial_registry_.end())
    return nullptr;
  return &pair->second;
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
  tutorial_registry_.emplace(id, std::move(description));
}

void TutorialRegistry::RemoveTutorialForTesting(TutorialIdentifier id) {
  tutorial_registry_.erase(id);
}

}  // namespace user_education
