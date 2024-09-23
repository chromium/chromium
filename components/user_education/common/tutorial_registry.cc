// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_education/common/tutorial_registry.h"

#include <memory>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/dcheck_is_on.h"
#include "base/ranges/algorithm.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_description.h"
#include "components/user_education/common/tutorial_identifier.h"

namespace user_education {

TutorialRegistry::TutorialRegistry() = default;
TutorialRegistry::~TutorialRegistry() = default;

bool TutorialRegistry::IsTutorialRegistered(TutorialIdentifier id) const {
  return base::Contains(tutorial_registry_, id);
}

const TutorialDescription* TutorialRegistry::GetTutorialDescription(
    TutorialIdentifier id) const {
  DCHECK(tutorial_registry_.size() > 0);
  auto pair = tutorial_registry_.find(id);
  if (pair == tutorial_registry_.end())
    return nullptr;
  return &pair->second;
}

const std::vector<TutorialIdentifier> TutorialRegistry::GetTutorialIdentifiers()
    const {
  DCHECK(tutorial_registry_.size() > 0);
  std::vector<TutorialIdentifier> id_strings;
  base::ranges::transform(tutorial_registry_, std::back_inserter(id_strings),
                          &Registry::value_type::first);
  return id_strings;
}

void TutorialRegistry::AddTutorial(TutorialIdentifier id,
                                   TutorialDescription description) {
  if (base::Contains(tutorial_registry_, id)) {
    return;
  }

  tutorial_registry_.emplace(id, std::move(description));
}

void TutorialRegistry::RemoveTutorialForTesting(TutorialIdentifier id) {
  tutorial_registry_.erase(id);
}

}  // namespace user_education
