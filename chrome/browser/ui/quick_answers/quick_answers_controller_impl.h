// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_card_controller.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/gfx/geometry/rect.h"

class Profile;
class QuickAnswersUiController;

// Implementation of QuickAnswerController. It fetches quick answers
// result via QuickAnswersClient and manages quick answers UI.
class QuickAnswersControllerImpl : public chromeos::ReadWriteCardController,
                                   public QuickAnswersController,
                                   public quick_answers::QuickAnswersDelegate {
 public:
  using TimeTickNowFunction = base::RepeatingCallback<base::TimeTicks()>;

  explicit QuickAnswersControllerImpl(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller);
  QuickAnswersControllerImpl(
      chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller,
      std::unique_ptr<QuickAnswersState> quick_answers_state);
  QuickAnswersControllerImpl(const QuickAnswersControllerImpl&) = delete;
  QuickAnswersControllerImpl& operator=(const QuickAnswersControllerImpl&) =
      delete;
  ~QuickAnswersControllerImpl() override;

  // chromeos::ReadWriteCardController:
  void OnContextMenuShown(Profile* profile) override;
  void OnTextAvailable(const gfx::Rect& anchor_bounds,
                       const std::string& selected_text,
                       const std::string& surrounding_text) override;
  void OnAnchorBoundsChanged(const gfx::Rect& anchor_bounds) override;
  void OnDismiss(bool is_other_command_executed) override;

  // QuickAnswersController:
  // SetClient is required to be called before using these methods.
  // TODO(yanxiao): refactor to delegate to browser.
  void SetClient(
      std::unique_ptr<quick_answers::QuickAnswersClient> client) override;
  quick_answers::QuickAnswersClient* GetClient() const override;
  void DismissQuickAnswers(
      quick_answers::QuickAnswersExitPoint exit_point) override;
  quick_answers::QuickAnswersDelegate* GetQuickAnswersDelegate() override;

  QuickAnswersVisibility GetQuickAnswersVisibility() const override;
  void SetVisibility(QuickAnswersVisibility visibility) override;

  // QuickAnswersDelegate:
  void OnQuickAnswerReceived(std::unique_ptr<quick_answers::QuickAnswersSession>
                                 quick_answers_session) override;
  void OnNetworkError() override;
  void OnRequestPreprocessFinished(
      const quick_answers::QuickAnswersRequest& processed_request) override;

  // Retry sending quick answers request to backend.
  void OnRetryQuickAnswersRequest();

  // User clicks on the quick answer result.
  void OnQuickAnswersResultClick();

  // Handle user consent result.
  void OnUserConsentResult(bool consented);

  void OverrideTimeTickNowForTesting(
      TimeTickNowFunction time_tick_now_function);

  QuickAnswersUiController* quick_answers_ui_controller() {
    return quick_answers_ui_controller_.get();
  }

  // `quick_answers_session()` return non-nullptr if it has received a result,
  // including `kNoResult`. `quick_answer()` return non-nullptr if it has
  // received a result which is NOT `kNoResult`;
  quick_answers::QuickAnswersSession* quick_answers_session() {
    return quick_answers_session_.get();
  }

  quick_answers::QuickAnswer* quick_answer() {
    return quick_answers_session_ ? quick_answers_session_->quick_answer.get()
                                  : nullptr;
  }

  quick_answers::StructuredResult* structured_result() {
    return quick_answers_session_
               ? quick_answers_session_->structured_result.get()
               : nullptr;
  }

  chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller() {
    return read_write_cards_ui_controller_.get();
  }

  base::WeakPtr<QuickAnswersControllerImpl> GetWeakPtr();

  const gfx::Rect& anchor_bounds() { return anchor_bounds_; }

 private:
  friend class QuickAnswersUiControllerTest;

  void HandleQuickAnswerRequest(
      const quick_answers::QuickAnswersRequest& request);

  // Returns true if a consent view has shown by a call. Otherwise returns
  // false.
  bool MaybeShowUserConsent(quick_answers::IntentType intent_type,
                            const std::u16string& intent_text);
  void OnUserConsent(ConsentResultType consent_result_type);

  base::TimeTicks GetTimeTicksNow();

  quick_answers::QuickAnswersRequest BuildRequest();

  // Profile that initiated the current query.
  raw_ptr<Profile> profile_ = nullptr;

  // Bounds of the anchor view.
  gfx::Rect anchor_bounds_;

  // Query used to retrieve quick answer.
  std::string query_;

  // Title to be shown on the QuickAnswers view.
  std::string title_;

  // Context information, including surrounding text and device properties.
  quick_answers::Context context_;

  // Time that the context menu is shown.
  base::TimeTicks menu_shown_time_;

  // Time that the consent ui is shown.
  base::TimeTicks consent_ui_shown_;

  // A fake time tick now function for testing. This must be null in production.
  TimeTickNowFunction time_tick_now_function_;

  std::unique_ptr<quick_answers::QuickAnswersClient> quick_answers_client_;

  std::unique_ptr<QuickAnswersState> quick_answers_state_;

  // The last received `QuickAnswersSession` from client.
  std::unique_ptr<quick_answers::QuickAnswersSession> quick_answers_session_;

  const raw_ref<chromeos::ReadWriteCardsUiController>
      read_write_cards_ui_controller_;

  // `quick_answers_ui_controller_` depends on `read_write_cards_ui_controller_`
  // via this controller. This has to be constructed-after and destructed-before
  // `read_write_cards_ui_controller_`.
  std::unique_ptr<QuickAnswersUiController> quick_answers_ui_controller_;

  QuickAnswersVisibility visibility_ = QuickAnswersVisibility::kClosed;

  // Use `std::unique_ptr` instead of `std::optional` as we can pass a class
  // defined in an unnamed namespace.
  std::unique_ptr<QuickAnswersStateObserver> perform_on_consent_accepted_;

  base::WeakPtrFactory<QuickAnswersControllerImpl> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_QUICK_ANSWERS_QUICK_ANSWERS_CONTROLLER_IMPL_H_
