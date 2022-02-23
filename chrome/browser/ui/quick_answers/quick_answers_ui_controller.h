// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

#include <string>

#include "ui/gfx/geometry/rect.h"

class QuickAnswersView;
class QuickAnswersControllerImpl;

namespace quick_answers {
class UserConsentView;
struct QuickAnswer;
}  // namespace quick_answers

// A controller to show/hide and handle interactions for quick
// answers view.
class QuickAnswersUiController {
 public:
  explicit QuickAnswersUiController(QuickAnswersControllerImpl* controller);
  ~QuickAnswersUiController();

  QuickAnswersUiController(const QuickAnswersUiController&) = delete;
  QuickAnswersUiController& operator=(const QuickAnswersUiController&) = delete;

  // Constructs/resets |quick_answers_view_|.
  void CreateQuickAnswersView(const gfx::Rect& anchor_bounds,
                              const std::string& title,
                              const std::string& query,
                              bool is_internal);

  // Returns true if there was a QuickAnswersView to close.
  bool CloseQuickAnswersView();

  void OnQuickAnswersViewPressed();

  void OnRetryLabelPressed();

  // |bounds| is the bound of context menu.
  void RenderQuickAnswersViewWithResult(
      const gfx::Rect& bounds,
      const quick_answers::QuickAnswer& quick_answer);

  void SetActiveQuery(const std::string& query);

  // Show retry option in the quick answers view.
  void ShowRetry();

  void UpdateQuickAnswersBounds(const gfx::Rect& anchor_bounds);

  // Creates a view for asking the user for consent about the Quick Answers
  // feature vertically aligned to the anchor.
  void CreateUserConsentView(const gfx::Rect& anchor_bounds,
                             const std::u16string& intent_type,
                             const std::u16string& intent_text);

  // Closes the user consent view.
  void CloseUserConsentView();

  // Used by the controller to check if the user consent view is currently
  // showing instead of QuickAnswers.
  bool is_showing_user_consent_view() const {
    return user_consent_view_ != nullptr;
  }

  // Used by the controller to check if the QuickAnswers view is currently
  // showing.
  bool is_showing_quick_answers_view() const {
    return quick_answers_view_ != nullptr;
  }

  // Invoked when user clicks the Dogfood button on Quick-Answers related views.
  void OnDogfoodButtonPressed();

  // Invoked when user clicks the settings button on Quick-Answers related
  // views.
  void OnSettingsButtonPressed();

  // Invoked when user clicks the report query button on Quick Answers view.
  void OnReportQueryButtonPressed();

  // Handle consent result from user consent view.
  void OnUserConsentResult(bool consented);

  const QuickAnswersView* quick_answers_view_for_testing() const {
    return quick_answers_view_;
  }
  const quick_answers::UserConsentView* consent_view_for_testing() const {
    return user_consent_view_;
  }

 private:
  QuickAnswersControllerImpl* controller_ = nullptr;

  // Owned by view hierarchy.
  QuickAnswersView* quick_answers_view_ = nullptr;
  quick_answers::UserConsentView* user_consent_view_ = nullptr;
  std::string query_;
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
