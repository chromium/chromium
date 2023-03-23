// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/gfx/geometry/rect.h"

class QuickAnswersState;
class QuickAnswersUiController;

// Implementation of QuickAnswerController. It fetches quick answers
// result via QuickAnswersClient and manages quick answers UI.
class QuickAnswersControllerImpl : public QuickAnswersController,
                                   public quick_answers::QuickAnswersDelegate {
 public:
  QuickAnswersControllerImpl();
  QuickAnswersControllerImpl(const QuickAnswersControllerImpl&) = delete;
  QuickAnswersControllerImpl& operator=(const QuickAnswersControllerImpl&) =
      delete;
  ~QuickAnswersControllerImpl() override;

  // QuickAnswersController:
  void SetClient(
      std::unique_ptr<quick_answers::QuickAnswersClient> client) override;

  // SetClient is required to be called before using these methods.
  // TODO(yanxiao): refactor to delegate to browser.
  void MaybeShowQuickAnswers(const gfx::Rect& anchor_bounds,
                             const std::string& title,
                             const quick_answers::Context& context) override;

  void DismissQuickAnswers(
      quick_answers::QuickAnswersExitPoint exit_point) override;

  // Update the bounds of the anchor view.
  void UpdateQuickAnswersAnchorBounds(const gfx::Rect& anchor_bounds) override;

  void SetPendingShowQuickAnswers() override;

  quick_answers::QuickAnswersDelegate* GetQuickAnswersDelegate() override;

  QuickAnswersVisibility GetVisibilityForTesting() const override;

  void SetVisibility(QuickAnswersVisibility visibility) override;

  // QuickAnswersDelegate:
  void OnQuickAnswerReceived(
      std::unique_ptr<quick_answers::QuickAnswer> answer) override;
  void OnNetworkError() override;
  void OnRequestPreprocessFinished(
      const quick_answers::QuickAnswersRequest& processed_request) override;

  // Retry sending quick answers request to backend.
  void OnRetryQuickAnswersRequest();

  // User clicks on the quick answer result.
  void OnQuickAnswerClick();

  // Handle user consent result.
  void OnUserConsentResult(bool consented);

  QuickAnswersUiController* quick_answers_ui_controller() {
    return quick_answers_ui_controller_.get();
  }

  quick_answers::QuickAnswer* quick_answer() { return quick_answer_.get(); }

 private:
  void HandleQuickAnswerRequest(
      const quick_answers::QuickAnswersRequest& request);

  // Show the user consent view. Does nothing if the view is already
  // visible.
  void ShowUserConsent(const std::u16string& intent_type,
                       const std::u16string& intent_text);

  quick_answers::QuickAnswersRequest BuildRequest();

  // Bounds of the anchor view.
  gfx::Rect anchor_bounds_;

  // Query used to retrieve quick answer.
  std::string query_;

  // Title to be shown on the QuickAnswers view.
  std::string title_;

  // Context information, including surrounding text and device properties.
  quick_answers::Context context_;

  std::unique_ptr<quick_answers::QuickAnswersClient> quick_answers_client_;

  std::unique_ptr<QuickAnswersState> quick_answers_state_;

  std::unique_ptr<QuickAnswersUiController> quick_answers_ui_controller_;

  // The last received QuickAnswer from client.
  std::unique_ptr<quick_answers::QuickAnswer> quick_answer_;

  QuickAnswersVisibility visibility_ = QuickAnswersVisibility::kClosed;
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
