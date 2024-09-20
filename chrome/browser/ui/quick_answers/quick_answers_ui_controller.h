// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

class Profile;
class QuickAnswersControllerImpl;

namespace chromeos {
class ReadWriteCardsUiController;
}  // namespace chromeos

namespace quick_answers {
class RichAnswersView;
class UserConsentView;
}  // namespace quick_answers

// A controller to show/hide and handle interactions for quick
// answers view.
class QuickAnswersUiController {
 public:
  using FakeOnRetryLabelPressedCallback = base::RepeatingCallback<void()>;

  // TODO(b/335701090): extract those browser actions to
  // QuickAnswersBrowserDelegate.
  using FakeOpenFeedbackPageCallback =
      base::RepeatingCallback<void(const std::string&)>;
  using FakeOpenWebUrlCallback = base::RepeatingCallback<void(const GURL&)>;

  explicit QuickAnswersUiController(QuickAnswersControllerImpl* controller);
  ~QuickAnswersUiController();

  QuickAnswersUiController(const QuickAnswersUiController&) = delete;
  QuickAnswersUiController& operator=(const QuickAnswersUiController&) = delete;

  // Constructs/resets the Quick Answers card view.
  void CreateQuickAnswersView(Profile* profile,
                              const std::string& title,
                              const std::string& query,
                              std::optional<quick_answers::Intent> intent,
                              QuickAnswersState::FeatureType feature_type,
                              bool is_internal);

  void CreateQuickAnswersViewForPixelTest(
      Profile* profile,
      const std::string& query,
      std::optional<quick_answers::Intent> intent,
      quick_answers::QuickAnswersView::Params params);

  // Returns true if there was a QuickAnswersView to close.
  bool CloseQuickAnswersView();

  // Returns true if there was a RichAnswersView to close.
  bool CloseRichAnswersView();

  void OnQuickAnswersViewPressed();

  void OnGoogleSearchLabelPressed();

  void OnRetryLabelPressed();
  void SetFakeOnRetryLabelPressedCallbackForTesting(
      FakeOnRetryLabelPressedCallback fake_on_retry_label_pressed_callback);

  void RenderQuickAnswersViewWithResult(
      const quick_answers::StructuredResult& structured_result);

  void SetActiveQuery(Profile* profile, const std::string& query);

  // Show retry option in the quick answers view.
  void ShowRetry();

  // Creates a view for asking the user for consent about the Quick Answers
  // feature vertically aligned to the anchor. Note that user consent is handled
  // by Quick Answers code only if `QuickAnswersState::FeatureType` is
  // `kQuickAnswers`.
  void CreateUserConsentView(const gfx::Rect& anchor_bounds,
                             quick_answers::IntentType intent_type,
                             const std::u16string& intent_text);
  void CreateUserConsentViewForPixelTest(const gfx::Rect& anchor_bounds,
                                         quick_answers::IntentType intent_type,
                                         const std::u16string& intent_text,
                                         bool use_refreshed_design);

  // Closes the user consent view.
  void CloseUserConsentView();

  // Invoked when user clicks the settings button on Quick-Answers related
  // views.
  void OnSettingsButtonPressed();

  // Invoked when user clicks the report query button on Quick Answers view.
  void OnReportQueryButtonPressed();
  void SetFakeOpenFeedbackPageCallbackForTesting(
      FakeOpenFeedbackPageCallback fake_open_feedback_page_callback);

  void SetFakeOpenWebUrlForTesting(
      FakeOpenWebUrlCallback fake_open_web_url_callback);

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

  // Gets the controller that is used to show the widget containing quick
  // answers views.
  chromeos::ReadWriteCardsUiController& GetReadWriteCardsUiController() const;

  quick_answers::QuickAnswersView* quick_answers_view() {
    return views::AsViewClass<quick_answers::QuickAnswersView>(
        quick_answers_view_.view());
  }
  quick_answers::UserConsentView* user_consent_view() {
    return views::AsViewClass<quick_answers::UserConsentView>(
        user_consent_view_.view());
  }
  quick_answers::RichAnswersView* rich_answers_view() {
    return views::AsViewClass<quick_answers::RichAnswersView>(
        rich_answers_widget_->GetContentsView());
  }

 private:
  void OpenFeedbackPage(const std::string& feedback_template);
  void OpenWebUrl(const GURL& url);
  void OnUserConsentNoThanksPressed();
  void OnUserConsentAllowPressed();

  void CreateQuickAnswersViewInternal(
      Profile* profile,
      const std::string& query,
      std::optional<quick_answers::Intent> intent,
      quick_answers::QuickAnswersView::Params params);
  void CreateUserConsentViewInternal(const gfx::Rect& anchor_bounds,
                                     quick_answers::IntentType intent_type,
                                     const std::u16string& intent_text,
                                     bool use_refreshed_design);

  // Constructs/resets the Quick Answers rich card view.
  void CreateRichAnswersView();

  raw_ptr<QuickAnswersControllerImpl> controller_ = nullptr;

  // Widget pointers for quick answers related views.
  views::UniqueWidgetPtr rich_answers_widget_;

  views::ViewTracker user_consent_view_;
  views::ViewTracker quick_answers_view_;

  raw_ptr<Profile> profile_ = nullptr;
  std::string query_;

  FakeOnRetryLabelPressedCallback fake_on_retry_label_pressed_callback_;
  FakeOpenFeedbackPageCallback fake_open_feedback_page_callback_;
  FakeOpenWebUrlCallback fake_open_web_url_callback_;

  base::WeakPtrFactory<QuickAnswersUiController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_UI_CONTROLLER_H_
