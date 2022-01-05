// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "base/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/quick_answers/ui/user_consent_view.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/escape.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

namespace {

using quick_answers::QuickAnswer;
using quick_answers::QuickAnswersExitPoint;

constexpr char kGoogleSearchUrlPrefix[] = "https://www.google.com/search?q=";

constexpr char kFeedbackDescriptionTemplate[] = "#QuickAnswers\nQuery:%s\n";

}  // namespace

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() {
  quick_answers_view_ = nullptr;
  user_consent_view_ = nullptr;
}

void QuickAnswersUiController::CreateQuickAnswersView(const gfx::Rect& bounds,
                                                      const std::string& title,
                                                      const std::string& query,
                                                      bool is_internal) {
  // Currently there are timing issues that causes the quick answers view is not
  // dismissed. TODO(updowndota): Remove the special handling after the root
  // cause is found.
  if (quick_answers_view_) {
    LOG(ERROR) << "Quick answers view not dismissed.";
    CloseQuickAnswersView();
  }

  DCHECK(!user_consent_view_);
  SetActiveQuery(query);
  quick_answers_view_ = new QuickAnswersView(bounds, title, is_internal, this);
  quick_answers_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kQuickAnswersClick);

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(kGoogleSearchUrlPrefix +
           net::EscapeUrlEncodedData(query_, /*use_plus=*/true)),
      /*from_user_interaction=*/true);
  controller_->OnQuickAnswerClick();
}

bool QuickAnswersUiController::CloseQuickAnswersView() {
  if (quick_answers_view_) {
    quick_answers_view_->GetWidget()->Close();
    quick_answers_view_ = nullptr;
    return true;
  }
  return false;
}

void QuickAnswersUiController::OnRetryLabelPressed() {
  controller_->OnRetryQuickAnswersRequest();
}

void QuickAnswersUiController::RenderQuickAnswersViewWithResult(
    const gfx::Rect& anchor_bounds,
    const QuickAnswer& quick_answer) {
  if (!quick_answers_view_)
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view_->UpdateView(anchor_bounds, quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(const std::string& query) {
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!quick_answers_view_)
    return;

  quick_answers_view_->ShowRetryView();
}

void QuickAnswersUiController::UpdateQuickAnswersBounds(
    const gfx::Rect& anchor_bounds) {
  if (quick_answers_view_)
    quick_answers_view_->UpdateAnchorViewBounds(anchor_bounds);

  if (user_consent_view_)
    user_consent_view_->UpdateAnchorViewBounds(anchor_bounds);
}

void QuickAnswersUiController::CreateUserConsentView(
    const gfx::Rect& anchor_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  DCHECK(!quick_answers_view_);
  DCHECK(!user_consent_view_);
  user_consent_view_ = new quick_answers::UserConsentView(
      anchor_bounds, intent_type, intent_text, this);
  user_consent_view_->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::CloseUserConsentView() {
  if (user_consent_view_) {
    user_consent_view_->GetWidget()->Close();
    user_consent_view_ = nullptr;
  }
}

void QuickAnswersUiController::OnSettingsButtonPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kSettingsButtonClick);

  controller_->OpenQuickAnswersSettings();
}

void QuickAnswersUiController::OnReportQueryButtonPressed() {
  controller_->DismissQuickAnswers(
      QuickAnswersExitPoint::kReportQueryButtonClick);

  ash::NewWindowDelegate::GetPrimary()->OpenFeedbackPage(
      ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers,
      base::StringPrintf(kFeedbackDescriptionTemplate, query_.c_str()));
}

void QuickAnswersUiController::OnUserConsentResult(bool consented) {
  DCHECK(user_consent_view_);
  controller_->OnUserConsentResult(consented);

  if (consented && quick_answers_view_)
    quick_answers_view_->RequestFocus();
}
