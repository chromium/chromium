// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_REGISTRY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_REGISTRY_H_

#include "chrome/browser/ui/user_education/help_bubble_factory_registry.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"

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
  const std::vector<TutorialIdentifier> GetTutorialIdentifiers();

  // Gets the TutorialDescription from the registry. Returns nullptr if
  // there is no registered tutorial under the given ID.
  TutorialDescription* GetTutorialDescription(TutorialIdentifier id);

  // Adds a TutorialID, TutorialDescription pair to the registry. This should
  // be used by the RegisterTutorials method to Add Tutorials.
  void AddTutorial(TutorialIdentifier id, TutorialDescription description);

  // Removes a TutorialID and its associated TutorialDescription from the
  // registry, to clean up from tests that need to inject test-only tutorials.
  void RemoveTutorialForTesting(TutorialIdentifier id);

 private:
  std::map<TutorialIdentifier, TutorialDescription> tutorial_registry_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_REGISTRY_H_
