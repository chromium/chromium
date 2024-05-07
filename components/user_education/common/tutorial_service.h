// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_SERVICE_H_
#define COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_SERVICE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "ui/base/interaction/element_tracker.h"

// Declare in the global scope for testing purposes.
class TutorialInteractiveUitest;

namespace user_education {

class HelpBubbleFactoryRegistry;
class TutorialRegistry;

namespace internal {
class TutorialStepBuilder;
}

// A profile based service which provides the current running tutorial. A
// TutorialService should be constructed by a factory which fills in the correct
// tutorials based on the platform the tutorial targets.
class TutorialService {
 public:
  TutorialService(TutorialRegistry* tutorial_registry,
                  HelpBubbleFactoryRegistry* help_bubble_factory_registry);
  virtual ~TutorialService();

  using CompletedCallback = base::OnceClosure;
  using AbortedCallback = base::OnceClosure;
  using RestartedCallback = base::RepeatingClosure;

  // Returns true if there is a currently running tutorial.
  // If `id` is specified, specifically returns whether *that* tutorial is
  // running.
  virtual bool IsRunningTutorial(
      std::optional<TutorialIdentifier> id = std::nullopt) const;

  // Cancels the tutorial `id` if it is running; or any tutorial if `id` is
  // not specified. Returns whether a tutorial was canceled.
  bool CancelTutorialIfRunning(
      std::optional<TutorialIdentifier> id = std::nullopt);

  // Sets the current help bubble stored by the service.
  void SetCurrentBubble(std::unique_ptr<HelpBubble> bubble, bool is_last_step);

  // Starts the tutorial by looking for the id in the Tutorial Registry.
  // Any existing tutorial is canceled.
  virtual void StartTutorial(
      TutorialIdentifier id,
      ui::ElementContext context,
      CompletedCallback completed_callback = base::DoNothing(),
      AbortedCallback aborted_callback = base::DoNothing(),
      RestartedCallback restart_callback = base::DoNothing());

  void LogIPHLinkClicked(TutorialIdentifier id, bool iph_link_was_clicked);
  virtual void LogStartedFromWhatsNewPage(TutorialIdentifier id,
                                          bool iph_link_was_clicked);

  // Accessors for registries.
  TutorialRegistry* tutorial_registry() { return tutorial_registry_; }
  HelpBubbleFactoryRegistry* bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }

  // Accessors for the help bubble used in tests.
  HelpBubble* currently_displayed_bubble_for_testing() {
    return currently_displayed_bubble_.get();
  }

 protected:
  // Returns application-specific strings.
  virtual std::u16string GetBodyIconAltText(bool is_last_step) const = 0;

 private:
  friend class Tutorial;
  friend class internal::TutorialStepBuilder;
  friend TutorialInteractiveUitest;

  // Uses the stored tutorial creation params to restart a tutorial. Replaces
  // the current_tutorial with a newly generated tutorial.
  bool RestartTutorial();

  // Calls the abort code for the running tutorial.
  void AbortTutorial(std::optional<int> abort_step);

  // Hides the current help bubble currently being shown by the service.
  void HideCurrentBubbleIfShowing();

  // Struct used to reconstruct a tutorial from the params initially used to
  // create it.
  struct TutorialCreationParams {
    TutorialCreationParams(const TutorialDescription* description,
                           ui::ElementContext context);

    raw_ptr<const TutorialDescription, DanglingUntriaged> description_;
    ui::ElementContext context_;
  };

  // Called when a non-final bubble is closed. Used to trigger the broken
  // tutorial timeout.
  void OnNonFinalBubbleClosed(HelpBubble* bubble,
                              HelpBubble::CloseReason reason);

  // Calls the completion code for the running tutorial.
  // TODO (dpenning): allow for registering a callback that performs any
  // IPH/other code on completion of tutorial
  void CompleteTutorial();

  // Reset all of the running tutorial member variables.
  void ResetRunningTutorial();

  // Called when there has been no bubble visible for enough time that the
  // current tutorial should probably be aborted.
  void OnBrokenTutorial();

  // Creation params for the last started tutorial. Used to restart the
  // tutorial after it has been completed.
  std::unique_ptr<TutorialCreationParams> running_tutorial_creation_params_;

  // The current tutorial created by the start or restart methods. This
  // tutorial is not required to have it's interaction sequence started to
  // be stored in the service.
  std::unique_ptr<Tutorial> running_tutorial_;

  // Set to the ID of the current or most recent tutorial to run.
  TutorialIdentifier most_recent_tutorial_id_;

  // Was restarted denotes that the current running tutorial was restarted,
  // and when logging that the tutorial aborts, instead should log as completed.
  bool running_tutorial_was_restarted_ = false;

  // Called when the current tutorial is completed.
  CompletedCallback completed_callback_ = base::DoNothing();

  // Called if the current tutorial is aborted.
  AbortedCallback aborted_callback_ = base::DoNothing();

  // Called if the current tutorial is restarted.
  RestartedCallback restarted_callback_ = base::DoNothing();

  // The current help bubble displayed by the tutorial. This is owned by the
  // service so that when the tutorial exits, the bubble can continue existing.
  std::unique_ptr<HelpBubble> currently_displayed_bubble_;

  // Listens for when the final bubble in a Tutorial is closed.
  base::CallbackListSubscription bubble_closed_subscription_;

  // Whether the currently-displayed bubble is the final one.
  bool is_final_bubble_ = false;

  // Used to check for broken tutorials - when no bubble is visible for an
  // unexpected period of time.
  base::OneShotTimer broken_tutorial_timer_;

  // Pointers to the registries used for constructing and showing tutorials and
  // help bubbles.
  const raw_ptr<TutorialRegistry> tutorial_registry_;
  const raw_ptr<HelpBubbleFactoryRegistry> help_bubble_factory_registry_;

  // status bit to denote that the tutorial service is in the process of
  // restarting a tutorial. This prevents calling the abort callbacks.
  bool is_restarting_ = false;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_TUTORIAL_SERVICE_H_
