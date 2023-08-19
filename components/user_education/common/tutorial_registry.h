// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_REGISTRY_H_
#define COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_REGISTRY_H_

#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_identifier.h"

namespace user_education {

class Tutorial;

// A TutorialRegistry is a Map of TutorialIdentifier to TutorialDescription.
// TutorialDescriptions should be added to the registry by subclassing the
// registry and overriding the RegisterTutorials method.
class TutorialRegistry {
 public:
  TutorialRegistry();
  ~TutorialRegistry();
  TutorialRegistry(const TutorialRegistry&) = delete;
  TutorialRegistry& operator=(const TutorialRegistry&) = delete;

  // Determines whether the given tutorial is registered.
  bool IsTutorialRegistered(TutorialIdentifier id) const;

  // Returns a list of Tutorial Identifiers if the tutorial registry exists.
  // If RegisterTutorials has not been called this returns an empty vector.
  const std::vector<TutorialIdentifier> GetTutorialIdentifiers() const;

  // Gets the TutorialDescription from the registry. Returns nullptr if
  // there is no registered tutorial under the given ID.
  const TutorialDescription* GetTutorialDescription(
      TutorialIdentifier id) const;

  // Adds a TutorialID, TutorialDescription pair to the registry. This should
  // be used by the RegisterTutorials method to Add Tutorials.
  void AddTutorial(TutorialIdentifier id, TutorialDescription description);

  // Removes a TutorialID and its associated TutorialDescription from the
  // registry, to clean up from tests that need to inject test-only tutorials.
  void RemoveTutorialForTesting(TutorialIdentifier id);

 private:
  using Registry = std::map<TutorialIdentifier, TutorialDescription>;

  Registry tutorial_registry_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_REGISTRY_H_
