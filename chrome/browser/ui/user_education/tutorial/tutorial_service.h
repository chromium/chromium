// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/callback_helpers.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial.h"
#include "chrome/browser/ui/user_education/tutorial/tutorial_identifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/base/interaction/element_tracker.h"

class HelpBubble;
class HelpBubbleFactoryRegistry;
class TutorialRegistry;

// A non-singleton service which provides the current running tutorial,
// registries for Tutorials. A TutorialService should be constructed by a
// factory which fills in the correct tutorials based on the platform the
// tutorial targets.
class TutorialService : public KeyedService {
 public:
  TutorialService(TutorialRegistry* tutorial_registry,
                  HelpBubbleFactoryRegistry* help_bubble_factory_registry);
  ~TutorialService() override;

  using CompletedCallback = base::RepeatingClosure;
  using AbortedCallback = base::RepeatingClosure;

  // returns true if there is a currently running tutorial.
  bool IsRunningTutorial() const;

  void SetCurrentBubble(std::unique_ptr<HelpBubble> bubble);

  void HideCurrentBubbleIfShowing();

  // Starts the tutorial by looking for the id in the Tutorial Registry.
  bool StartTutorial(TutorialIdentifier id, ui::ElementContext context);

  void SetOnCompleteTutorialForTesting(CompletedCallback callback);
  void SetOnAbortTutorialForTesting(AbortedCallback callback);

  TutorialRegistry* tutorial_registry() { return tutorial_registry_; }
  HelpBubbleFactoryRegistry* bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }

 private:
  friend class Tutorial;
  friend class TutorialInteractiveUitest;

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

  // a function to call on complete of the tutorial
  CompletedCallback completed_callback_ = base::DoNothing();
  AbortedCallback aborted_callback_ = base::DoNothing();

  std::unique_ptr<HelpBubble> currently_displayed_bubble_;

  TutorialRegistry* const tutorial_registry_;
  HelpBubbleFactoryRegistry* const help_bubble_factory_registry_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_
