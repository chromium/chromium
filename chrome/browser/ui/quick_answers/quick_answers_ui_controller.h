// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

class QuickAnswersView;
class QuickAnswersControllerImpl;

namespace quick_answers {
class RichAnswersView;
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

  // Constructs/resets the Quick Answers card view.
  void CreateQuickAnswersView(const gfx::Rect& anchor_bounds,
                              const std::string& title,
                              const std::string& query,
                              bool is_internal);

  // Returns true if there was a QuickAnswersView to close.
  bool CloseQuickAnswersView();

  // Returns true if there was a RichAnswersView to close.
  bool CloseRichAnswersView();

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

  // Invoked when user clicks the Dogfood button on Quick-Answers related views.
  void OnDogfoodButtonPressed();

  // Invoked when user clicks the settings button on Quick-Answers related
  // views.
  void OnSettingsButtonPressed();

  // Invoked when user clicks the report query button on Quick Answers view.
  void OnReportQueryButtonPressed();

  // Handle consent result from user consent view.
  void OnUserConsentResult(bool consented);

  // Used by the controller to check if the user consent view is currently
  // showing instead of QuickAnswers.
  bool IsShowingUserConsentView() const;

  // Used by the controller to check if the QuickAnswers view is currently
  // showing.
  bool IsShowingQuickAnswersView() const;

  // Used by the controller to check if the RichAnswers view is currently
  // showing.
  bool IsShowingRichAnswersView() const;

  quick_answers::QuickAnswersView* quick_answers_view() {
    return static_cast<quick_answers::QuickAnswersView*>(
        quick_answers_widget_->GetContentsView());
  }
  quick_answers::UserConsentView* user_consent_view() {
    return static_cast<quick_answers::UserConsentView*>(
        user_consent_widget_->GetContentsView());
  }
  quick_answers::RichAnswersView* rich_answers_view() {
    return static_cast<quick_answers::RichAnswersView*>(
        rich_answers_widget_->GetContentsView());
  }

 private:
  // Constructs/resets the Quick Answers rich card view.
  void CreateRichAnswersView();

  raw_ptr<QuickAnswersControllerImpl> controller_ = nullptr;

  // Widget pointers for quick answers related views.
  views::UniqueWidgetPtr quick_answers_widget_;
  views::UniqueWidgetPtr user_consent_widget_;
  views::UniqueWidgetPtr rich_answers_widget_;

  std::string query_;

  base::WeakPtrFactory<QuickAnswersUiController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
