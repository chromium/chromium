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
}  // namespace quick_answers

enum class QuickAnswersVisibility {
  // Quick Answers UI is hidden and the previous session has finished.
  kClosed = 0,
  // Quick Answers session is initializing and the UI will be shown when the
  // context is ready.
  kPending = 1,
  // Quick Answers UI is visible.
  kQuickAnswersVisible = 2,
  // User Consent UI is visible.
  kUserConsentVisible = 3,
  // Rich Answers UI is visible.
  kRichAnswersVisible = 4,
};

// A controller to manage Quick Answers UI. This controller manages the
// Quick Answers requests and high-level UI handling such as creation/dismissal.
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
  virtual quick_answers::QuickAnswersClient* GetClient() const = 0;

  // Dismiss the specific quick-answers, user-consent, or rich-answers view
  // currently shown. |exit_point| indicates the exit point of the view.
  virtual void DismissQuickAnswers(
      quick_answers::QuickAnswersExitPoint exit_point) = 0;

  virtual quick_answers::QuickAnswersDelegate* GetQuickAnswersDelegate() = 0;

  virtual QuickAnswersVisibility GetQuickAnswersVisibility() const = 0;

  virtual void SetVisibility(QuickAnswersVisibility visibility) = 0;
};

#endif  // CHROMEOS_COMPONENTS_QUICK_ANSWERS_PUBLIC_CPP_CONTROLLER_QUICK_ANSWERS_CONTROLLER_H_
