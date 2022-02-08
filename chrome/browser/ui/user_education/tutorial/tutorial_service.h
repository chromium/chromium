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

  using CompletedCallback = base::OnceClosure;
  using AbortedCallback = base::OnceClosure;

  // returns true if there is a currently running tutorial.
  bool IsRunningTutorial() const;

  void SetCurrentBubble(std::unique_ptr<HelpBubble> bubble);

  void HideCurrentBubbleIfShowing();

  // Starts the tutorial by looking for the id in the Tutorial Registry.
  bool StartTutorial(TutorialIdentifier id,
                     ui::ElementContext context,
                     CompletedCallback completed_callback = base::DoNothing(),
                     AbortedCallback aborted_callback = base::DoNothing());

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

  // The current running tutorial.
  std::unique_ptr<Tutorial> running_tutorial_;

  // Called when the current tutorial is completed.
  CompletedCallback completed_callback_ = base::DoNothing();

  // Called if the current tutorial is aborted.
  AbortedCallback aborted_callback_ = base::DoNothing();

  std::unique_ptr<HelpBubble> currently_displayed_bubble_;

  TutorialRegistry* const tutorial_registry_;
  HelpBubbleFactoryRegistry* const help_bubble_factory_registry_;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_
