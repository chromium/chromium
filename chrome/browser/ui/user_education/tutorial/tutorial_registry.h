// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_REGISTRY_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_REGISTRY_H_

#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble_factory_registry.h"
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

  // Returns a list of Tutorial Identifiers if the tutorial registry exists.
  // If RegisterTutorials has not been called this returns an empty vector.
  const std::vector<TutorialIdentifier> GetTutorialIdentifiers();

  // Constructs the tutorial from the TutorialDescription at a given ID.
  // If no TutorialIdentifier can be found that matches the ID, a nullptr
  // is returned. The TutorialService is passed in order to allow Tutorials
  // to be constructed with access to the BubbleOwnerRegistry.
  std::unique_ptr<Tutorial> CreateTutorial(
      TutorialIdentifier id,
      TutorialService* tutorial_service,
      TutorialBubbleFactoryRegistry* bubble_factory_registry,
      ui::ElementContext context);

  // Adds a TutorialID, TutorialDescription Pair to the Registry. This should
  // be used by the RegisterTutorials method to Add Tutorials.
  void AddTutorial(TutorialIdentifier id, TutorialDescription description);

 private:
  std::map<TutorialIdentifier, TutorialDescription> tutorial_registry_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_REGISTRY_H_
