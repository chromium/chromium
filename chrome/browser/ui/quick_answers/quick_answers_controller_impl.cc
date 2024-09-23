// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"

#include <memory>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/chromeos/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_client.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_controller.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/quick_answers/quick_answers_state_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/feedback_util.h"
#include "chrome/browser/ui/quick_answers/lacros/quick_answers_state_lacros.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using ::quick_answers::Context;
using ::quick_answers::IntentType;
using ::quick_answers::QuickAnswer;
using ::quick_answers::QuickAnswersClient;
using ::quick_answers::QuickAnswersExitPoint;
using ::quick_answers::QuickAnswersRequest;
using ::quick_answers::ResultType;

constexpr char kQuickAnswersExitPoint[] = "QuickAnswers.ExitPoint";
constexpr char kQuickAnswersConsent[] = "QuickAnswers.V2.Consent";
constexpr char kQuickAnswersConsentImpression[] =
    "QuickAnswers.V2.Consent.Impression";
constexpr char kQuickAnswersConsentDuration[] =
    "QuickAnswers.V2.Consent.Duration";

std::string ConsentResultTypeToString(ConsentResultType type) {
  switch (type) {
    case ConsentResultType::kAllow:
      return "Allow";
    case ConsentResultType::kNoThanks:
      return "NoThanks";
    case ConsentResultType::kDismiss:
      return "Dismiss";
  }
}

void RecordConsentUiHistograms(ConsentResultType consent_result_type,
                               int32_t impression_count,
                               const base::TimeDelta& ui_duration) {
  base::UmaHistogramExactLinear(kQuickAnswersConsent, impression_count,
                                kConsentImpressionCap);

  std::string interaction_type = ConsentResultTypeToString(consent_result_type);
  base::UmaHistogramExactLinear(
      base::StringPrintf("%s.%s", kQuickAnswersConsentImpression,
                         interaction_type.c_str()),
      impression_count, kConsentImpressionCap);
  base::UmaHistogramTimes(
      base::StringPrintf("%s.%s", kQuickAnswersConsentDuration,
                         interaction_type.c_str()),
      ui_duration);
}

// Returns if the request has already been processed (by the text annotator).
bool IsProcessedRequest(const QuickAnswersRequest& request) {
  return (request.preprocessed_output.intent_info.intent_type !=
          quick_answers::IntentType::kUnknown);
}

bool ShouldShowQuickAnswers() {
  if (!QuickAnswersState::IsEligible()) {
    return false;
  }

  if (QuickAnswersState::IsEnabled()) {
    return true;
  }

  // If feature type is `kQuickAnswers`, return `true` for the case `kUnknown`
  // to show a consent UI.
  if (QuickAnswersState::GetFeatureType() ==
      QuickAnswersState::FeatureType::kQuickAnswers) {
    base::expected<quick_answers::prefs::ConsentStatus,
                   QuickAnswersState::Error>
        maybe_consent_status = QuickAnswersState::GetConsentStatus();
    if (!maybe_consent_status.has_value()) {
      return false;
    }

    if (maybe_consent_status.value() ==
        quick_answers::prefs::ConsentStatus::kUnknown) {
      return true;
    }
  }

  return false;
}

bool IsActiveUserInternal() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  auto* user = user_manager::UserManager::Get()->GetActiveUser();
  const std::string email = user->GetAccountId().GetUserEmail();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const std::string email = feedback_util::GetSignedInUserEmail();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return gaia::IsGoogleInternalAccountEmail(email);
}

// Error case might return nullptr(s). Consider an error case as no result.
// TODO(b/349920395): use std::variant<NoResult, DefinitionResult, ...> for
// structured result.
bool IsNoResult(
    const quick_answers::QuickAnswersSession* quick_answers_session) {
  if (!quick_answers_session) {
    return true;
  }

  if (!quick_answers_session->structured_result) {
    return true;
  }

  return quick_answers_session->structured_result->GetResultType() ==
         ResultType::kNoResult;
}

// TODO(b/340628526): This can be IsEnabled waiter as IsEnabled is gated by a
// consent status now.
class PerformOnConsentAccepted : public QuickAnswersStateObserver {
 public:
  explicit PerformOnConsentAccepted(base::OnceCallback<void()> action)
      : action_(std::move(action)) {
    CHECK(action_);

    // `QuickAnswersState::AddObserver` calls an added observer with a current
    // value (or a pref value later if it's not initialized yet).
    scoped_observation_.Observe(QuickAnswersState::Get());
  }

  // QuickAnswersStateObserver:
  void OnSettingsEnabled(bool enabled) override { MaybeRun(); }

  void OnConsentStatusUpdated(
      quick_answers::prefs::ConsentStatus consent_status) override {
    MaybeRun();
  }

 private:
  void MaybeRun() {
    if (!action_) {
      return;
    }

    bool settings_enabled = QuickAnswersState::IsEnabledAs(
        QuickAnswersState::FeatureType::kQuickAnswers);
    if (!settings_enabled) {
      return;
    }

    if (QuickAnswersState::GetConsentStatusAs(
            QuickAnswersState::FeatureType::kQuickAnswers) !=
        quick_answers::prefs::ConsentStatus::kAccepted) {
      return;
    }

    scoped_observation_.Reset();
    std::move(action_).Run();
  }

  base::ScopedObservation<QuickAnswersState, PerformOnConsentAccepted>
      scoped_observation_{this};
  base::OnceCallback<void()> action_;
};

std::unique_ptr<QuickAnswersState> CreateQuickAnswersState() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return std::make_unique<QuickAnswersStateAsh>();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::make_unique<QuickAnswersStateLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

QuickAnswersControllerImpl::QuickAnswersControllerImpl(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller)
    : QuickAnswersControllerImpl(read_write_cards_ui_controller,
                                 CreateQuickAnswersState()) {}

QuickAnswersControllerImpl::QuickAnswersControllerImpl(
    chromeos::ReadWriteCardsUiController& read_write_cards_ui_controller,
    std::unique_ptr<QuickAnswersState> quick_answers_state)
    : quick_answers_state_(std::move(quick_answers_state)),
      read_write_cards_ui_controller_(read_write_cards_ui_controller),
      quick_answers_ui_controller_(
          std::make_unique<QuickAnswersUiController>(this)) {}

QuickAnswersControllerImpl::~QuickAnswersControllerImpl() {
  // `PerformOnConsentAccepted` depends on `QuickAnswersState`. It has to be
  // destructed before `QuickAnswersState`.
  perform_on_consent_accepted_.reset();

  quick_answers_client_.reset();
  quick_answers_state_.reset();
}

void QuickAnswersControllerImpl::OnContextMenuShown(Profile* profile) {
  menu_shown_time_ = base::TimeTicks::Now();
  visibility_ = QuickAnswersVisibility::kPending;
  profile_ = profile;
}

void QuickAnswersControllerImpl::OnTextAvailable(
    const gfx::Rect& anchor_bounds,
    const std::string& selected_text,
    const std::string& surrounding_text) {
  if (!ShouldShowQuickAnswers())
    return;

  if (visibility_ != QuickAnswersVisibility::kPending) {
    return;
  }

  Context context;
  context.surrounding_text = surrounding_text;
  context.device_properties.is_internal = IsActiveUserInternal();

  // Cache anchor-bounds and query.
  anchor_bounds_ = anchor_bounds;
  // Initially, title is same as query. Title and query can be overridden based
  // on text annotation result at |OnRequestPreprocessFinish|.
  title_ = selected_text;
  query_ = selected_text;
  context_ = context;
  quick_answers_session_.reset();

  QuickAnswersRequest request = BuildRequest();
  if (QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    // Send the request for preprocessing. Only shows quick answers view if the
    // predicted intent is not |kUnknown| at |OnRequestPreprocessFinish|.
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    HandleQuickAnswerRequest(request);
  }
}

void QuickAnswersControllerImpl::OnAnchorBoundsChanged(
    const gfx::Rect& anchor_bounds) {
  anchor_bounds_ = anchor_bounds;
}

void QuickAnswersControllerImpl::OnDismiss(bool is_other_command_executed) {
  const base::TimeDelta time_since_request_sent =
      base::TimeTicks::Now() - menu_shown_time_;
  if (is_other_command_executed) {
    base::UmaHistogramTimes("QuickAnswers.ContextMenu.Close.DurationWithClick",
                            time_since_request_sent);
  } else {
    base::UmaHistogramTimes(
        "QuickAnswers.ContextMenu.Close.DurationWithoutClick",
        time_since_request_sent);
  }

  base::UmaHistogramBoolean("QuickAnswers.ContextMenu.Close",
                            is_other_command_executed);

  QuickAnswersExitPoint exit_point =
      is_other_command_executed ? QuickAnswersExitPoint::kContextMenuClick
                                : QuickAnswersExitPoint::kContextMenuDismiss;
  DismissQuickAnswers(exit_point);

  profile_ = nullptr;
}

void QuickAnswersControllerImpl::SetClient(
    std::unique_ptr<QuickAnswersClient> client) {
  quick_answers_client_ = std::move(client);
}

QuickAnswersClient* QuickAnswersControllerImpl::GetClient() const {
  return quick_answers_client_.get();
}

void QuickAnswersControllerImpl::DismissQuickAnswers(
    QuickAnswersExitPoint exit_point) {
  switch (visibility_) {
    case QuickAnswersVisibility::kRichAnswersVisible: {
      // For the rich-answers view, ignore dismissal by context-menu related
      // actions as they should only affect the companion quick-answers views.
      if (exit_point == QuickAnswersExitPoint::kContextMenuDismiss ||
          exit_point == QuickAnswersExitPoint::kContextMenuClick) {
        return;
      }
      quick_answers_ui_controller_->CloseRichAnswersView();
      visibility_ = QuickAnswersVisibility::kClosed;
      return;
    }
    case QuickAnswersVisibility::kUserConsentVisible: {
      // TODO(b/340628526): Replace IsShowingUserConsentView condition and
      // visibility_ write with CHECK.
      if (quick_answers_ui_controller_->IsShowingUserConsentView()) {
        OnUserConsent(ConsentResultType::kDismiss);
      }
      visibility_ = QuickAnswersVisibility::kClosed;
      return;
    }
    case QuickAnswersVisibility::kQuickAnswersVisible:
    case QuickAnswersVisibility::kPending:
    case QuickAnswersVisibility::kClosed: {
      bool closed = quick_answers_ui_controller_->CloseQuickAnswersView();
      visibility_ = QuickAnswersVisibility::kClosed;
      // |quick_answers_session_| could be null before we receive the result
      // from the server. Do not send the signal since the quick answer is
      // dismissed before ready.
      if (quick_answers_session_ && quick_answer()) {
        // For quick-answer rendered along with browser context menu, if user
        // didn't click on other context menu items, it is considered as active
        // impression.
        bool is_active = exit_point != QuickAnswersExitPoint::kContextMenuClick;
        quick_answers_client_->OnQuickAnswersDismissed(
            quick_answer()->result_type, is_active && closed);

        // Record Quick Answers exit point.
        // Make sure |closed| is true so that only the direct exit point is
        // recorded when multiple dismiss requests are received (For example,
        // dismiss request from context menu will also fire when the settings
        // button is pressed).
        if (closed) {
          base::UmaHistogramEnumeration(kQuickAnswersExitPoint, exit_point);
        }
      }
      return;
    }
  }
}

void QuickAnswersControllerImpl::HandleQuickAnswerRequest(
    const quick_answers::QuickAnswersRequest& request) {
  base::expected<quick_answers::prefs::ConsentStatus, QuickAnswersState::Error>
      maybe_consent_status = QuickAnswersState::GetConsentStatus();
  if (!maybe_consent_status.has_value()) {
    // No UI should be shown at this point, i.e., there should be no need to
    // reset UI. Reset is done in `OnTextAvailable` by a next request.
    // TODO(b/352469160): move those states to `QuickAnswersSession` as we can
    // easily reset a state.
    return;
  }

  switch (maybe_consent_status.value()) {
    case quick_answers::prefs::ConsentStatus::kRejected:
      CHECK(false) << "No request should be made if kRejected.";
      return;
    case quick_answers::prefs::ConsentStatus::kUnknown:
      MaybeShowUserConsent(
          request.preprocessed_output.intent_info.intent_type,
          base::UTF8ToUTF16(
              request.preprocessed_output.intent_info.intent_text));
      return;
    case quick_answers::prefs::ConsentStatus::kAccepted:
      visibility_ = QuickAnswersVisibility::kQuickAnswersVisible;
      // TODO(b/327501381): Use `ReadWriteCardsUiController` for this view.
      quick_answers_ui_controller_->CreateQuickAnswersView(
          profile_, title_, query_,
          ToIntent(request.preprocessed_output.intent_info.intent_type),
          quick_answers_state_->GetFeatureType(),
          request.context.device_properties.is_internal);

      if (IsProcessedRequest(request)) {
        quick_answers_client_->FetchQuickAnswers(request);
      } else {
        quick_answers_client_->SendRequest(request);
      }
      return;
  }

  CHECK(false) << "Invalid ConsentStatus enum value provided.";
}

quick_answers::QuickAnswersDelegate*
QuickAnswersControllerImpl::GetQuickAnswersDelegate() {
  return this;
}

QuickAnswersVisibility QuickAnswersControllerImpl::GetQuickAnswersVisibility()
    const {
  return visibility_;
}

void QuickAnswersControllerImpl::SetVisibility(
    QuickAnswersVisibility visibility) {
  visibility_ = visibility;
}

void QuickAnswersControllerImpl::OnQuickAnswerReceived(
    std::unique_ptr<quick_answers::QuickAnswersSession> quick_answers_session) {
  if (visibility_ != QuickAnswersVisibility::kQuickAnswersVisible) {
    return;
  }

  quick_answers_session_ = std::move(quick_answers_session);

  if (IsNoResult(quick_answers_session_.get())) {
    // Fallback query to title if no result is available.
    query_ = title_;
    quick_answers_ui_controller_->SetActiveQuery(profile_, query_);

    // `quick_answers_session_` can be nullptr. Create an empty result session
    // for the case if nullptr.
    if (!quick_answers_session_) {
      quick_answers_session_ =
          std::make_unique<quick_answers::QuickAnswersSession>();
    }
    if (!quick_answers_session_->structured_result) {
      quick_answers_session_->structured_result =
          std::make_unique<quick_answers::StructuredResult>();
    }
  }

  quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
      *(quick_answers_session_->structured_result));
}

void QuickAnswersControllerImpl::OnNetworkError() {
  if (visibility_ != QuickAnswersVisibility::kQuickAnswersVisible) {
    return;
  }

  // Notify quick_answers_ui_controller_ to show retry UI.
  quick_answers_ui_controller_->ShowRetry();
}

void QuickAnswersControllerImpl::OnRequestPreprocessFinished(
    const QuickAnswersRequest& processed_request) {
  if (!QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    // Ignore preprocessing result if text annotator is not enabled.
    return;
  }

  auto intent_type =
      processed_request.preprocessed_output.intent_info.intent_type;

  if (intent_type == quick_answers::IntentType::kUnknown) {
    return;
  }

  auto* active_menu_controller = views::MenuController::GetActiveInstance();
  if (visibility_ == QuickAnswersVisibility::kClosed ||
      !active_menu_controller || !active_menu_controller->owner()) {
    return;
  }

  query_ = processed_request.preprocessed_output.query;
  title_ = processed_request.preprocessed_output.intent_info.intent_text;

  HandleQuickAnswerRequest(processed_request);
}

void QuickAnswersControllerImpl::OnRetryQuickAnswersRequest() {
  QuickAnswersRequest request = BuildRequest();
  if (QuickAnswersState::Get()->ShouldUseQuickAnswersTextAnnotator()) {
    quick_answers_client_->SendRequestForPreprocessing(request);
  } else {
    quick_answers_client_->SendRequest(request);
  }
}

void QuickAnswersControllerImpl::OnQuickAnswersResultClick() {
  CHECK(quick_answers_client_);
  CHECK(quick_answers_session_);
  CHECK(quick_answers_session_->structured_result);

  quick_answers_client_->OnQuickAnswerClick(
      quick_answers_session_->structured_result->GetResultType());
}

void QuickAnswersControllerImpl::OnUserConsent(
    ConsentResultType consent_result_type) {
  CHECK(!consent_ui_shown_.is_null()) << "Consent ui is not shown.";

  quick_answers_ui_controller_->CloseUserConsentView();

  QuickAnswersState* quick_answers_state = QuickAnswersState::Get();
  CHECK(quick_answers_state);

  // It's okay to initialize this as false since there is no chance that this
  // becomes true if `consent_ui_duration` is less than the minimum cap:
  // a. there is no increment for increment cap for the case.
  // b. consent ui should not be shown in the first place for the case.
  bool reached_impression_cap = false;

  base::TimeDelta consent_ui_duration = GetTimeTicksNow() - consent_ui_shown_;
  if (consent_ui_duration.InSeconds() >= kConsentImpressionMinimumDuration) {
    int incremented_count =
        quick_answers_state->AsyncIncrementImpressionCount();
    RecordConsentUiHistograms(consent_result_type, incremented_count,
                              consent_ui_duration);

    reached_impression_cap = incremented_count >= kConsentImpressionCap;
  }

  switch (consent_result_type) {
    case ConsentResultType::kAllow: {
      CHECK_EQ(QuickAnswersState::GetFeatureType(),
               QuickAnswersState::FeatureType::kQuickAnswers)
          << "User consent is handled by Magic Boost if not kQuickAnswers";

      visibility_ = QuickAnswersVisibility::kPending;
      quick_answers_state->AsyncSetConsentStatus(
          quick_answers::prefs::ConsentStatus::kAccepted);

      // Preference value can be updated as an async operation. Wait the value
      // change and then display quick answer for the cached query. There should
      // be no need to reset `perform_on_consent_accepted_` as there is no case
      // a user accepts a consent twice on a device. Toggling from OS settings
      // will set value directly to `kAccepted`.
      CHECK(!perform_on_consent_accepted_)
          << "There is already a pending action. A user should not accept "
             "a consent twice or more.";
      perform_on_consent_accepted_ =
          std::make_unique<PerformOnConsentAccepted>(base::BindOnce(
              &QuickAnswersControllerImpl::OnTextAvailable, GetWeakPtr(),
              anchor_bounds_, title_, context_.surrounding_text));
      break;
    }
    case ConsentResultType::kNoThanks: {
      visibility_ = QuickAnswersVisibility::kClosed;
      quick_answers_state->AsyncSetConsentStatus(
          quick_answers::prefs::ConsentStatus::kRejected);
      break;
    }
    case ConsentResultType::kDismiss:
      visibility_ = QuickAnswersVisibility::kClosed;
      if (reached_impression_cap) {
        quick_answers_state->AsyncSetConsentStatus(
            quick_answers::prefs::ConsentStatus::kRejected);
      }
      break;
  }
}

void QuickAnswersControllerImpl::OnUserConsentResult(bool consented) {
  OnUserConsent(consented ? ConsentResultType::kAllow
                          : ConsentResultType::kNoThanks);
}

base::TimeTicks QuickAnswersControllerImpl::GetTimeTicksNow() {
  if (time_tick_now_function_.is_null()) {
    return base::TimeTicks::Now();
  }

  CHECK_IS_TEST();
  return time_tick_now_function_.Run();
}

void QuickAnswersControllerImpl::OverrideTimeTickNowForTesting(
    TimeTickNowFunction time_tick_now_function) {
  CHECK_IS_TEST();
  CHECK(time_tick_now_function_.is_null());

  time_tick_now_function_ = time_tick_now_function;
}

base::WeakPtr<QuickAnswersControllerImpl>
QuickAnswersControllerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool QuickAnswersControllerImpl::MaybeShowUserConsent(
    IntentType intent_type,
    const std::u16string& intent_text) {
  // For non-QuickAnswers case (i.e., HMR), user consent is handled outside of
  // QuickAnswers code.
  if (QuickAnswersState::GetFeatureType() !=
      QuickAnswersState::FeatureType::kQuickAnswers) {
    return false;
  }

  if (quick_answers_ui_controller_->IsShowingUserConsentView()) {
    return false;
  }

  quick_answers_ui_controller_->CreateUserConsentView(anchor_bounds_,
                                                      intent_type, intent_text);

  consent_ui_shown_ = GetTimeTicksNow();
  visibility_ = QuickAnswersVisibility::kUserConsentVisible;

  return true;
}

QuickAnswersRequest QuickAnswersControllerImpl::BuildRequest() {
  QuickAnswersRequest request;
  request.selected_text = title_;
  request.context = context_;
  return request;
}
