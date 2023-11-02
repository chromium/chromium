// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_
#define CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

namespace quick_answers {
class QuickAnswersClient;
class QuickAnswersDelegate;
enum class QuickAnswersExitPoint;
struct Context;
}  // namespace quick_answers

enum class QuickAnswersVisibility {
  // Quick Answers UI is hidden and the previous session has finished.
  kClosed = 0,
  // Quick Answers session is initializing and the UI will be shown when the
  // context is ready.
  kPending = 1,
  // Quick Answers UI is visible.
  kVisible = 2,
};

// A controller to manage quick answers UI.
class QuickAnswersController {
 public:
  QuickAnswersController();
  virtual ~QuickAnswersController();

  // Get the instance of |QuickAnswersController|. It is only available when
  // quick answers rich UI is enabled.
  static QuickAnswersController* Get();

  // Passes in a client instance for the controller to use.
  virtual void SetClient(
      std::unique_ptr<quick_answers::QuickAnswersClient> client) = 0;

  // Show the quick-answers view (and/or any accompanying/associated views like
  // user-consent view instead, if consent is not yet granted). |anchor_bounds|
  // is the bounds of the anchor view (which is the context menu for browser).
  // |title| is the text selected by the user. |context| is the context
  // information which will be used as part of the request for getting more
  // relevant result.
  virtual void MaybeShowQuickAnswers(const gfx::Rect& anchor_bounds,
                                     const std::string& title,
                                     const quick_answers::Context& context) = 0;

  // Dismiss the quick-answers view (and/or any associated views like
  // user-consent view) currently shown. |exit_point| indicates the exit point
  // of the quick-answers view.
  virtual void DismissQuickAnswers(
      quick_answers::QuickAnswersExitPoint exit_point) = 0;

  // Update the bounds of the anchor view.
  virtual void UpdateQuickAnswersAnchorBounds(
      const gfx::Rect& anchor_bounds) = 0;

  // Called when a quick-answers session has started but the detailed context is
  // still pending.
  virtual void SetPendingShowQuickAnswers() = 0;

  virtual quick_answers::QuickAnswersDelegate* GetQuickAnswersDelegate() = 0;

  virtual QuickAnswersVisibility GetVisibilityForTesting() const = 0;
};

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_
