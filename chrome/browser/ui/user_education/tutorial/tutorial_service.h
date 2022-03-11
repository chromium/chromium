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

// A profile based service which provides the current running tutorial. A
// TutorialService should be constructed by a factory which fills in the correct
// tutorials based on the platform the tutorial targets.
class TutorialService : public KeyedService {
 public:
  TutorialService(TutorialRegistry* tutorial_registry,
                  HelpBubbleFactoryRegistry* help_bubble_factory_registry);
  ~TutorialService() override;

  using CompletedCallback = base::OnceClosure;
  using AbortedCallback = base::OnceClosure;

  // Returns true if there is a currently running tutorial.
  bool IsRunningTutorial() const;

  // Sets the current help bubble stored by the service.
  void SetCurrentBubble(std::unique_ptr<HelpBubble> bubble);

  // Hides the current help bubble currently being shown by the service.
  void HideCurrentBubbleIfShowing();

  // Starts the tutorial by looking for the id in the Tutorial Registry.
  bool StartTutorial(TutorialIdentifier id,
                     ui::ElementContext context,
                     CompletedCallback completed_callback = base::DoNothing(),
                     AbortedCallback aborted_callback = base::DoNothing());

  // Uses the stored tutorial creation params to restart a tutorial. Replaces
  // the current_tutorial with a newly generated tutorial.
  bool RestartTutorial();

  // Accessors for registries.
  TutorialRegistry* tutorial_registry() { return tutorial_registry_; }
  HelpBubbleFactoryRegistry* bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }

  // Accessors for the help bubble used in tests.
  HelpBubble* currently_displayed_bubble() {
    return currently_displayed_bubble_.get();
  }

 private:
  friend class Tutorial;
  friend class TutorialInteractiveUitest;

  // Struct used to reconstruct a tutorial from the params initially used to
  // create it.
  struct TutorialCreationParams {
    TutorialCreationParams(TutorialDescription* description,
                           ui::ElementContext context);

    TutorialDescription* description_;
    ui::ElementContext context_;
  };

  // Calls the abort code for the running tutorial.
  void AbortTutorial();

  // Calls the completion code for the running tutorial.
  // TODO (dpenning): allow for registering a callback that performs any
  // IPH/other code on completion of tutorial
  void CompleteTutorial();

  // Reset all of the running tutorial member variables.
  void ResetRunningTutorial();

  // Creation params for the last started tutorial. Used to restart the
  // tutorial after it has been completed.
  std::unique_ptr<TutorialCreationParams> running_tutorial_creation_params_;

  // The current tutorial created by the start or restart methods. This
  // tutorial is not required to have it's interaction sequence started to
  // be stored in the service.
  std::unique_ptr<Tutorial> running_tutorial_;

  // Was restarted denotes that the current running tutorial was restarted,
  // and when logging that the tutorial aborts, instead should log as completed.
  bool running_tutorial_was_restarted_ = false;

  // Called when the current tutorial is completed.
  CompletedCallback completed_callback_ = base::DoNothing();

  // Called if the current tutorial is aborted.
  AbortedCallback aborted_callback_ = base::DoNothing();

  // The current help bubble displayed by the tutorial. This is owned by the
  // service so that when the tutorial exits, the bubble can continue existing.
  std::unique_ptr<HelpBubble> currently_displayed_bubble_;

  // Pointers to the registries used for constructing and showing tutorials and
  // help bubbles.
  TutorialRegistry* const tutorial_registry_;
  HelpBubbleFactoryRegistry* const help_bubble_factory_registry_;

  // status bit to denote that the tutorial service is in the process of
  // restarting a tutorial. This prevents calling the abort callbacks.
  bool is_restarting_ = false;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_TUTORIAL_TUTORIAL_SERVICE_H_
