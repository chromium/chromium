// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_bubble.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_registry.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/interaction/element_tracker.h"

// A non-singleton service which provides the current running tutorial,
// registries for Tutorials. A TutorialService should be constructed by a
// factory which fills in the correct tutorials based on the platform the
// tutorial targets.
class TutorialService : public KeyedService {
 public:
  TutorialService();
  ~TutorialService() override;

  using CompletedCallback = base::RepeatingClosure;
  using AbortedCallback = base::RepeatingClosure;

  // returns true if there is a currently running tutorial.
  bool IsRunningTutorial() const;

  void SetCurrentBubble(std::unique_ptr<TutorialBubble> bubble);

  void HideCurrentBubbleIfShowing();

  // Returns a list of Tutorial Identifiers if the tutorial registry exists.
  // if there is no registry this returns an empty vector.
  std::vector<TutorialIdentifier> GetTutorialIdentifiers() const;

  // Starts the tutorial by looking for the id in the Tutorial Registry.
  bool StartTutorial(
      TutorialIdentifier id,
      ui::ElementContext context,
      TutorialBubbleFactoryRegistry* bubble_factory_registry = nullptr,
      TutorialRegistry* tutorial_registry = nullptr);

  void SetOnCompleteTutorial(CompletedCallback callback);
  void SetOnAbortTutorial(AbortedCallback callback);

 private:
  friend class Tutorial;

  // Aborts the current running tutorial if there is one.
  void AbortTutorial();

  // Clears out the current tutorial.
  // TODO (dpenning): allow for registering a callback that performs any
  // IPH/other code on completion of tutorial
  void CompleteTutorial();

  // Starts the tutorial and returns true if a tutorial was started.
  bool StartTutorialImpl(std::unique_ptr<Tutorial> tutorial);

  // The current running tutorial.
  std::unique_ptr<Tutorial> running_tutorial_;

  // The current bubble.
  std::unique_ptr<TutorialBubble> currently_displayed_bubble_;

  // a function to call on complete of the tutorial
  CompletedCallback completed_callback_;
  AbortedCallback aborted_callback_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_
