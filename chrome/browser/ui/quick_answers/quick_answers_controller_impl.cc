// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"

#include "base/metrics/histogram_functions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_prefs.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_controller.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/quick_answers/quick_answers_state_ash.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
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

std::u16string IntentTypeToString(IntentType intent_type) {
  switch (intent_type) {
    case IntentType::kUnit:
      return l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_UNIT_CONVERSION_INTENT);
    case IntentType::kDictionary:
      return l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_DEFINITION_INTENT);
    case IntentType::kTranslation:
      return l10n_util::GetStringUTF16(IDS_QUICK_ANSWERS_TRANSLATION_INTENT);
    case IntentType::kUnknown:
      return std::u16string();
  }
}

// Returns if the request has already been processed (by the text annotator).
bool IsProcessedRequest(const QuickAnswersRequest& request) {
  return (request.preprocessed_output.intent_info.intent_type !=
          quick_answers::IntentType::kUnknown);
}

bool ShouldShowQuickAnswers() {
  if (!QuickAnswersState::Get()->is_eligible())
    return false;

  bool settings_enabled = QuickAnswersState::Get()->settings_enabled();

  bool should_show_consent = QuickAnswersState::Get()->consent_status() ==
                             quick_answers::prefs::ConsentStatus::kUnknown;
  return settings_enabled || should_show_consent;
}

}  // namespace

QuickAnswersControllerImpl::QuickAnswersControllerImpl()
    : quick_answers_ui_controller_(
          std::make_unique<QuickAnswersUiController>(this)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  quick_answers_state_ = std::make_unique<QuickAnswersStateAsh>();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  quick_answers_state_ = std::make_unique<QuickAnswersStateLacros>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

QuickAnswersControllerImpl::~QuickAnswersControllerImpl() {
  quick_answers_client_.reset();
  quick_answers_state_.reset();
}

void QuickAnswersControllerImpl::SetClient(
    std::unique_ptr<QuickAnswersClient> client) {
  quick_answers_client_ = std::move(client);
}

void QuickAnswersControllerImpl::MaybeShowQuickAnswers(
    const gfx::Rect& anchor_bounds,
    const std::string& title,
    const Context& context) {
  if (!ShouldShowQuickAnswers())
    return;

  if (visibility_ != QuickAnswersVisibility::kPending) {
    return;
  }

  // Cache anchor-bounds and query.
  anchor_bounds_ = anchor_bounds;
  // Initially, title is same as query. Title and query can be overridden based
  // on text annotation result at |OnRequestPreprocessFinish|.
  title_ = title;
  query_ = title;
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

void QuickAnswersControllerImpl::HandleQuickAnswerRequest(
    const quick_answers::QuickAnswersRequest& request) {
  if (QuickAnswersState::Get()->consent_status() ==
      quick_answers::prefs::ConsentStatus::kUnknown) {
    ShowUserConsent(
        IntentTypeToString(request.preprocessed_output.intent_info.intent_type),
        base::UTF8ToUTF16(request.preprocessed_output.intent_info.intent_text));
  } else {
    visibility_ = QuickAnswersVisibility::kQuickAnswersVisible;
    quick_answers_ui_controller_->CreateQuickAnswersView(
        anchor_bounds_, title_, query_,
        request.context.device_properties.is_internal);

    if (IsProcessedRequest(request))
      quick_answers_client_->FetchQuickAnswers(request);
    else
      quick_answers_client_->SendRequest(request);
  }
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
      if (quick_answers_ui_controller_->IsShowingUserConsentView()) {
        QuickAnswersState::Get()->OnConsentResult(ConsentResultType::kDismiss);
      }
      quick_answers_ui_controller_->CloseUserConsentView();
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

quick_answers::QuickAnswersDelegate*
QuickAnswersControllerImpl::GetQuickAnswersDelegate() {
  return this;
}

QuickAnswersVisibility QuickAnswersControllerImpl::GetVisibilityForTesting()
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

  if (quick_answer()) {
    if (quick_answer()->title.empty()) {
      quick_answer()->title.push_back(
          std::make_unique<quick_answers::QuickAnswerText>(title_));
    }
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, *quick_answer());
  } else {
    quick_answers::QuickAnswer quick_answer_with_no_result;
    quick_answer_with_no_result.title.push_back(
        std::make_unique<quick_answers::QuickAnswerText>(title_));
    quick_answer_with_no_result.first_answer_row.push_back(
        std::make_unique<quick_answers::QuickAnswerResultText>(
            l10n_util::GetStringUTF8(IDS_QUICK_ANSWERS_VIEW_NO_RESULT_V2)));
    quick_answers_ui_controller_->RenderQuickAnswersViewWithResult(
        anchor_bounds_, quick_answer_with_no_result);
    // Fallback query to title if no result is available.
    query_ = title_;
    quick_answers_ui_controller_->SetActiveQuery(query_);
  }
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

void QuickAnswersControllerImpl::OnQuickAnswerClick() {
  quick_answers_client_->OnQuickAnswerClick(
      quick_answer() ? quick_answer()->result_type : ResultType::kNoResult);
}

void QuickAnswersControllerImpl::UpdateQuickAnswersAnchorBounds(
    const gfx::Rect& anchor_bounds) {
  anchor_bounds_ = anchor_bounds;
  quick_answers_ui_controller_->UpdateQuickAnswersBounds(anchor_bounds);
}

void QuickAnswersControllerImpl::SetPendingShowQuickAnswers() {
  visibility_ = QuickAnswersVisibility::kPending;
}

void QuickAnswersControllerImpl::OnUserConsentResult(bool consented) {
  quick_answers_ui_controller_->CloseUserConsentView();

  QuickAnswersState::Get()->OnConsentResult(
      consented ? ConsentResultType::kAllow : ConsentResultType::kNoThanks);

  if (consented) {
    visibility_ = QuickAnswersVisibility::kPending;
    // Display Quick-Answer for the cached query when user consent has
    // been granted.
    MaybeShowQuickAnswers(anchor_bounds_, title_, context_);
  }
}

void QuickAnswersControllerImpl::ShowUserConsent(
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  // Show consent informing user about the feature if required.
  if (!quick_answers_ui_controller_->IsShowingUserConsentView()) {
    quick_answers_ui_controller_->CreateUserConsentView(
        anchor_bounds_, intent_type, intent_text);
    QuickAnswersState::Get()->StartConsent();
    visibility_ = QuickAnswersVisibility::kUserConsentVisible;
  }
}

QuickAnswersRequest QuickAnswersControllerImpl::BuildRequest() {
  QuickAnswersRequest request;
  request.selected_text = title_;
  request.context = context_;
  return request;
}
