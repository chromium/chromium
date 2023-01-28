// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"

#include "base/functional/bind.h"
#include "base/strings/escape.h"
#include "base/strings/stringprintf.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/quick_answers/ui/rich_answers_view.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/new_window_delegate.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_commands.h"          // nogncheck
#include "chrome/browser/ui/browser_finder.h"            // nogncheck
#include "chrome/browser/ui/browser_navigator.h"         // nogncheck
#include "chrome/browser/ui/browser_navigator_params.h"  // nogncheck
#include "chromeos/crosapi/mojom/url_handler.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

using quick_answers::QuickAnswer;
using quick_answers::QuickAnswersExitPoint;

constexpr char kGoogleSearchUrlPrefix[] = "https://www.google.com/search?q=";
constexpr char kGoogleTranslateUrlTemplate[] =
    "https://translate.google.com/?sl=auto&tl=%s&text=%s&op=translate";

constexpr char kFeedbackDescriptionTemplate[] = "#QuickAnswers\nQuery:%s\n";
constexpr char kTranslationQueryPrefix[] = "Translate:";

constexpr char kQuickAnswersSettingsUrl[] =
    "chrome://os-settings/osSearch/search";

// Open the specified URL in a new tab in the primary browser.
void OpenUrl(const GURL& url) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetInstance()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  NavigateParams navigate_params(
      profile, url,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_FROM_API));
  navigate_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  navigate_params.window_action = NavigateParams::SHOW_WINDOW;
  Navigate(&navigate_params);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() = default;

void QuickAnswersUiController::CreateQuickAnswersView(const gfx::Rect& bounds,
                                                      const std::string& title,
                                                      const std::string& query,
                                                      bool is_internal) {
  // Currently there are timing issues that causes the quick answers view is not
  // dismissed. TODO(updowndota): Remove the special handling after the root
  // cause is found.
  if (IsShowingQuickAnswersView()) {
    LOG(ERROR) << "Quick answers view not dismissed.";
    CloseQuickAnswersView();
  }

  DCHECK(!IsShowingUserConsentView());
  SetActiveQuery(query);

  // Owned by view hierarchy.
  auto* const quick_answers_view = new QuickAnswersView(
      bounds, title, is_internal, weak_factory_.GetWeakPtr());
  quick_answers_view_tracker_.SetView(quick_answers_view);
  quick_answers_view->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  if (chromeos::features::IsQuickAnswersRichCardEnabled()) {
    auto* const rich_answers_view =
        new RichAnswersView(quick_answers_view_tracker_.view()->bounds(),
                            weak_factory_.GetWeakPtr());
    rich_answers_view_tracker_.SetView(rich_answers_view);
    rich_answers_view->GetWidget()->ShowInactive();
    controller_->DismissQuickAnswers(QuickAnswersExitPoint::kQuickAnswersClick);
    return;
  }

  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kQuickAnswersClick);

  // TODO(b/240619915): Refactor so that we can access the request metadata
  // instead of just the query itself.
  if (base::StartsWith(query_, kTranslationQueryPrefix)) {
    auto query_text = base::EscapeUrlEncodedData(
        query_.substr(strlen(kTranslationQueryPrefix)), /*use_plus=*/true);
    auto device_language =
        l10n_util::GetLanguage(QuickAnswersState::Get()->application_locale());
    auto translate_url =
        base::StringPrintf(kGoogleTranslateUrlTemplate, device_language.c_str(),
                           query_text.c_str());
    OpenUrl(GURL(translate_url));
  } else {
    OpenUrl(GURL(kGoogleSearchUrlPrefix +
                 base::EscapeUrlEncodedData(query_, /*use_plus=*/true)));
  }
  controller_->OnQuickAnswerClick();
}

bool QuickAnswersUiController::CloseQuickAnswersView() {
  if (IsShowingQuickAnswersView()) {
    quick_answers_view()->GetWidget()->Close();
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
  if (!IsShowingQuickAnswersView())
    return;

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view()->UpdateView(anchor_bounds, quick_answer);
}

void QuickAnswersUiController::SetActiveQuery(const std::string& query) {
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!IsShowingQuickAnswersView())
    return;

  quick_answers_view()->ShowRetryView();
}

void QuickAnswersUiController::UpdateQuickAnswersBounds(
    const gfx::Rect& anchor_bounds) {
  if (IsShowingQuickAnswersView())
    quick_answers_view()->UpdateAnchorViewBounds(anchor_bounds);

  if (IsShowingUserConsentView())
    user_consent_view()->UpdateAnchorViewBounds(anchor_bounds);
}

void QuickAnswersUiController::CreateUserConsentView(
    const gfx::Rect& anchor_bounds,
    const std::u16string& intent_type,
    const std::u16string& intent_text) {
  DCHECK(!IsShowingQuickAnswersView());
  DCHECK(!IsShowingUserConsentView());

  // Owned by view hierarchy.
  auto* const user_consent_view = new quick_answers::UserConsentView(
      anchor_bounds, intent_type, intent_text, weak_factory_.GetWeakPtr());
  user_consent_view_tracker_.SetView(user_consent_view);
  user_consent_view->GetWidget()->ShowInactive();
}

void QuickAnswersUiController::CloseUserConsentView() {
  if (IsShowingUserConsentView()) {
    user_consent_view()->GetWidget()->Close();
  }
}

void QuickAnswersUiController::OnSettingsButtonPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kSettingsButtonClick);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  OpenUrl(GURL(kQuickAnswersSettingsUrl));
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // OS settings app is implemented in Ash, but OpenUrl here does not qualify
  // for redirection in Lacros due to security limitations. Thus we need to
  // explicitly send the request to Ash to launch the OS settings app.
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  DCHECK(service->IsAvailable<crosapi::mojom::UrlHandler>());

  service->GetRemote<crosapi::mojom::UrlHandler>()->OpenUrl(
      GURL(kQuickAnswersSettingsUrl));
#endif
}

void QuickAnswersUiController::OnReportQueryButtonPressed() {
  controller_->DismissQuickAnswers(
      QuickAnswersExitPoint::kReportQueryButtonClick);

  auto feedback_template =
      base::StringPrintf(kFeedbackDescriptionTemplate, query_.c_str());

  // TODO(b/229007013): Merge the logics after resolve the deps cycle with
  // //c/b/ui in ash chrome build.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::NewWindowDelegate::GetPrimary()->OpenFeedbackPage(
      ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers,
      feedback_template);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  chrome::OpenFeedbackDialog(
      chrome::FindBrowserWithActiveWindow(),
      chrome::FeedbackSource::kFeedbackSourceQuickAnswers, feedback_template);
#endif
}

void QuickAnswersUiController::OnUserConsentResult(bool consented) {
  DCHECK(IsShowingUserConsentView());
  controller_->OnUserConsentResult(consented);

  if (consented && IsShowingQuickAnswersView())
    quick_answers_view()->RequestFocus();
}

bool QuickAnswersUiController::IsShowingUserConsentView() const {
  return user_consent_view_tracker_.view() &&
         !user_consent_view_tracker_.view()->GetWidget()->IsClosed();
}

bool QuickAnswersUiController::IsShowingQuickAnswersView() const {
  return quick_answers_view_tracker_.view() &&
         !quick_answers_view_tracker_.view()->GetWidget()->IsClosed();
}
