// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/quick_answers/quick_answers_ui_controller.h"

#include <optional>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/webui/settings/public/constants/routes.mojom-forward.h"
#include "ash/webui/settings/public/constants/setting.mojom-shared.h"
#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/quick_answers/quick_answers_controller_impl.h"
#include "chrome/browser/ui/ash/quick_answers/ui/quick_answers_util.h"
#include "chrome/browser/ui/ash/quick_answers/ui/quick_answers_view.h"
#include "chrome/browser/ui/ash/quick_answers/ui/rich_answers_definition_view.h"
#include "chrome/browser/ui/ash/quick_answers/ui/rich_answers_translation_view.h"
#include "chrome/browser/ui/ash/quick_answers/ui/rich_answers_unit_conversion_view.h"
#include "chrome/browser/ui/ash/quick_answers/ui/rich_answers_view.h"
#include "chrome/browser/ui/ash/quick_answers/ui/user_consent_view.h"
#include "chrome/browser/ui/ash/read_write_cards/read_write_cards_ui_controller.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chromeos/components/quick_answers/public/cpp/constants.h"
#include "chromeos/components/quick_answers/public/cpp/controller/quick_answers_controller.h"
#include "chromeos/components/quick_answers/public/cpp/quick_answers_state.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/metadata/view_factory_internal.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

using quick_answers::QuickAnswer;
using quick_answers::QuickAnswersExitPoint;

constexpr char kFeedbackDescriptionTemplate[] = "#QuickAnswers\nQuery:%s\n";

// TODO(b/365588558, crbug.com/374253370): `OSSettingsType` and `ShowOSSettings`
// are to avoid having ash dependency from lacros build. Delete those code once
// we can delete lacros code.
enum class OSSettingsType { QuickAnswers, Mahi };

void ShowOSSettings(Profile* profile, OSSettingsType type) {
  switch (type) {
    case OSSettingsType::QuickAnswers:
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chromeos::settings::mojom::kSearchSubpagePath,
          chromeos::settings::mojom::Setting::kQuickAnswersOnOff);
      return;
    case OSSettingsType::Mahi:
      chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
          profile, chromeos::settings::mojom::kSystemPreferencesSectionPath,
          chromeos::settings::mojom::Setting::kMahiOnOff);
      return;
  }

  CHECK(false) << "Invalid os settings type provided";
}

// Open the specified URL in a new tab with the specified profile
void OpenUrl(Profile* profile, const GURL& url) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

quick_answers::Design GetDesign(QuickAnswersState::FeatureType feature_type) {
  switch (feature_type) {
    case QuickAnswersState::FeatureType::kQuickAnswers:
      return chromeos::features::IsQuickAnswersMaterialNextUIEnabled()
                 ? quick_answers::Design::kRefresh
                 : quick_answers::Design::kCurrent;
    case QuickAnswersState::FeatureType::kHmr:
      return quick_answers::Design::kMagicBoost;
  }

  CHECK(false) << "Invalid feature type enum value provided";
}

}  // namespace

using chromeos::ReadWriteCardsUiController;

QuickAnswersUiController::QuickAnswersUiController(
    QuickAnswersControllerImpl* controller)
    : controller_(controller) {}

QuickAnswersUiController::~QuickAnswersUiController() {
  // Created Quick Answers UIs (e.g., `UserConsentView`) can have dependency to
  // `QuickAnswersUiController`. Destruct those UIs before destructing a UI
  // controller. Note that `RemoveQuickAnswersUi` is no-op if no Quick Answers
  // UI is currently shown.
  GetReadWriteCardsUiController().RemoveQuickAnswersUi();
}

void QuickAnswersUiController::CreateQuickAnswersView(
    Profile* profile,
    const std::string& title,
    const std::string& query,
    std::optional<quick_answers::Intent> intent,
    QuickAnswersState::FeatureType feature_type,
    bool is_internal) {
  CreateQuickAnswersViewInternal(profile, query, intent,
                                 {
                                     .title = title,
                                     .design = GetDesign(feature_type),
                                     .is_internal = is_internal,
                                 });
}

void QuickAnswersUiController::CreateQuickAnswersViewForPixelTest(
    Profile* profile,
    const std::string& query,
    std::optional<quick_answers::Intent> intent,
    quick_answers::QuickAnswersView::Params params) {
  CHECK_IS_TEST();
  CreateQuickAnswersViewInternal(profile, query, intent, params);
}

void QuickAnswersUiController::CreateQuickAnswersViewInternal(
    Profile* profile,
    const std::string& query,
    std::optional<quick_answers::Intent> intent,
    quick_answers::QuickAnswersView::Params params) {
  // Currently there are timing issues that causes the quick answers view is not
  // dismissed. TODO(updowndota): Remove the special handling after the root
  // cause is found.
  if (IsShowingQuickAnswersView()) {
    LOG(ERROR) << "Quick answers view not dismissed.";
    CloseQuickAnswersView();
  }

  DCHECK(!IsShowingUserConsentView());
  SetActiveQuery(profile, query);

  auto* view = GetReadWriteCardsUiController().SetQuickAnswersUi(
      views::Builder<quick_answers::QuickAnswersView>(
          std::make_unique<quick_answers::QuickAnswersView>(
              params,
              /*controller=*/weak_factory_.GetWeakPtr()))
          .CustomConfigure(base::BindOnce(
              [](std::optional<quick_answers::Intent> intent,
                 quick_answers::QuickAnswersView* quick_answers_view) {
                if (intent) {
                  quick_answers_view->SetIntent(intent.value());
                }
              },
              intent))
          .Build());

  quick_answers_view_.SetView(view);
}

void QuickAnswersUiController::CreateRichAnswersView() {
  CHECK(controller_->quick_answer());

  views::UniqueWidgetPtr widget = quick_answers::RichAnswersView::CreateWidget(
      controller_->anchor_bounds(), weak_factory_.GetWeakPtr(),
      *controller_->quick_answer(), *controller_->structured_result());

  if (!widget) {
    // If the rich card widget cannot be created, fall-back to open the query
    // in Google Search.
    OpenUrl(profile_, quick_answers::GetDetailsUrlForQuery(query_));
    controller_->OnQuickAnswersResultClick();
  }

  rich_answers_widget_ = std::move(widget);
  rich_answers_widget_->Show();
  controller_->SetVisibility(QuickAnswersVisibility::kRichAnswersVisible);
  return;
}

void QuickAnswersUiController::OnQuickAnswersViewPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kQuickAnswersClick);

  // Trigger the corresponding rich card view if the feature is enabled.
  if (chromeos::features::IsQuickAnswersRichCardEnabled() &&
      controller_->quick_answer() != nullptr) {
    CreateRichAnswersView();
    return;
  }

  OpenWebUrl(quick_answers::GetDetailsUrlForQuery(query_));

  if (controller_->quick_answers_session()) {
    controller_->OnQuickAnswersResultClick();
  }
}

void QuickAnswersUiController::OnGoogleSearchLabelPressed() {
  OpenWebUrl(quick_answers::GetDetailsUrlForQuery(query_));

  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kUnspecified);
}

bool QuickAnswersUiController::CloseQuickAnswersView() {
  if (controller_->GetQuickAnswersVisibility() ==
      QuickAnswersVisibility::kQuickAnswersVisible) {
    GetReadWriteCardsUiController().RemoveQuickAnswersUi();
    return true;
  }
  return false;
}

bool QuickAnswersUiController::CloseRichAnswersView() {
  if (IsShowingRichAnswersView()) {
    rich_answers_widget_->Close();
    return true;
  }
  return false;
}

void QuickAnswersUiController::OnRetryLabelPressed() {
  if (!fake_on_retry_label_pressed_callback_.is_null()) {
    CHECK_IS_TEST();
    fake_on_retry_label_pressed_callback_.Run();
    return;
  }

  controller_->OnRetryQuickAnswersRequest();
}

void QuickAnswersUiController::SetFakeOnRetryLabelPressedCallbackForTesting(
    QuickAnswersUiController::FakeOnRetryLabelPressedCallback
        fake_on_retry_label_pressed_callback) {
  CHECK_IS_TEST();
  CHECK(!fake_on_retry_label_pressed_callback.is_null());
  CHECK(fake_on_retry_label_pressed_callback_.is_null());
  fake_on_retry_label_pressed_callback_ = fake_on_retry_label_pressed_callback;
}

void QuickAnswersUiController::RenderQuickAnswersViewWithResult(
    const quick_answers::StructuredResult& structured_result) {
  if (!IsShowingQuickAnswersView()) {
    return;
  }

  // QuickAnswersView was initiated with a loading page and will be updated
  // when quick answers result from server side is ready.
  quick_answers_view()->SetResult(structured_result);
}

void QuickAnswersUiController::SetActiveQuery(Profile* profile,
                                              const std::string& query) {
  profile_ = profile;
  query_ = query;
}

void QuickAnswersUiController::ShowRetry() {
  if (!IsShowingQuickAnswersView()) {
    return;
  }

  quick_answers_view()->ShowRetryView();
}

void QuickAnswersUiController::CreateUserConsentView(
    const gfx::Rect& anchor_bounds,
    quick_answers::IntentType intent_type,
    const std::u16string& intent_text) {
  CreateUserConsentViewInternal(
      anchor_bounds, intent_type, intent_text,
      /*use_refreshed_design=*/
      chromeos::features::IsQuickAnswersMaterialNextUIEnabled());
}

void QuickAnswersUiController::CreateUserConsentViewForPixelTest(
    const gfx::Rect& anchor_bounds,
    quick_answers::IntentType intent_type,
    const std::u16string& intent_text,
    bool use_refreshed_design) {
  CHECK_IS_TEST();
  CreateUserConsentViewInternal(anchor_bounds, intent_type, intent_text,
                                use_refreshed_design);
}

void QuickAnswersUiController::CreateUserConsentViewInternal(
    const gfx::Rect& anchor_bounds,
    quick_answers::IntentType intent_type,
    const std::u16string& intent_text,
    bool use_refreshed_design) {
  CHECK_EQ(controller_->GetQuickAnswersVisibility(),
           QuickAnswersVisibility::kPending);

  auto* view = GetReadWriteCardsUiController().SetQuickAnswersUi(
      views::Builder<quick_answers::UserConsentView>(
          std::make_unique<quick_answers::UserConsentView>(
              use_refreshed_design, GetReadWriteCardsUiController()))
          .SetIntentType(intent_type)
          .SetIntentText(intent_text)
          // It is safe to do `base::Unretained(this)`. UIs are destructed
          // before a UI controller gets destructed. See
          // `~QuickAnswersUiController`.
          .SetNoThanksButtonPressed(base::BindRepeating(
              &QuickAnswersUiController::OnUserConsentNoThanksPressed,
              base::Unretained(this)))
          .SetAllowButtonPressed(base::BindRepeating(
              &QuickAnswersUiController::OnUserConsentAllowPressed,
              base::Unretained(this)))
          .Build());
  user_consent_view_.SetView(view);

  // `ViewAccessibility::AnnounceText` requires a root view. Announce text after
  // a view gets attached to a widget.
  user_consent_view_.view()->GetViewAccessibility().AnnounceText(
      l10n_util::GetStringUTF16(
          IDS_QUICK_ANSWERS_USER_NOTICE_VIEW_A11Y_INFO_ALERT_TEXT));
}

void QuickAnswersUiController::CloseUserConsentView() {
  CHECK_EQ(controller_->GetQuickAnswersVisibility(),
           QuickAnswersVisibility::kUserConsentVisible);
  GetReadWriteCardsUiController().RemoveQuickAnswersUi();
}

void QuickAnswersUiController::OnSettingsButtonPressed() {
  // Route dismissal through |controller_| for logging impressions.
  controller_->DismissQuickAnswers(QuickAnswersExitPoint::kSettingsButtonClick);

  switch (QuickAnswersState::GetFeatureType()) {
    case QuickAnswersState::FeatureType::kQuickAnswers:
      ShowOSSettings(profile_, OSSettingsType::QuickAnswers);
      return;
    case QuickAnswersState::FeatureType::kHmr:
      ShowOSSettings(profile_, OSSettingsType::Mahi);
      return;
  }

  CHECK(false) << "Invalid feature type enum value specified";
}

void QuickAnswersUiController::OnReportQueryButtonPressed() {
  controller_->DismissQuickAnswers(
      QuickAnswersExitPoint::kReportQueryButtonClick);

  OpenFeedbackPage(
      base::StringPrintf(kFeedbackDescriptionTemplate, query_.c_str()));
}

void QuickAnswersUiController::SetFakeOpenFeedbackPageCallbackForTesting(
    QuickAnswersUiController::FakeOpenFeedbackPageCallback
        fake_open_feedback_page_callback) {
  CHECK_IS_TEST();
  CHECK(!fake_open_feedback_page_callback.is_null());
  CHECK(fake_open_feedback_page_callback_.is_null());
  fake_open_feedback_page_callback_ = fake_open_feedback_page_callback;
}

void QuickAnswersUiController::OpenFeedbackPage(
    const std::string& feedback_template) {
  if (!fake_open_feedback_page_callback_.is_null()) {
    CHECK_IS_TEST();
    fake_open_feedback_page_callback_.Run(feedback_template);
    return;
  }

  // TODO(b/229007013, crbug.com/374253370): Merge the logics after resolve the
  // deps cycle with //c/b/ui in ash chrome build.
  ash::NewWindowDelegate::GetPrimary()->OpenFeedbackPage(
      ash::NewWindowDelegate::FeedbackSource::kFeedbackSourceQuickAnswers,
      feedback_template);
}

void QuickAnswersUiController::SetFakeOpenWebUrlForTesting(
    QuickAnswersUiController::FakeOpenWebUrlCallback
        fake_open_web_url_callback) {
  CHECK_IS_TEST();
  CHECK(!fake_open_web_url_callback.is_null());
  CHECK(fake_open_web_url_callback_.is_null());
  fake_open_web_url_callback_ = fake_open_web_url_callback;
}

void QuickAnswersUiController::OpenWebUrl(const GURL& url) {
  if (!fake_open_web_url_callback_.is_null()) {
    CHECK_IS_TEST();
    fake_open_web_url_callback_.Run(url);
    return;
  }

  OpenUrl(profile_, url);
}

void QuickAnswersUiController::OnUserConsentNoThanksPressed() {
  OnUserConsentResult(false);
}

void QuickAnswersUiController::OnUserConsentAllowPressed() {
  // When user consent is accepted, `QuickAnswersView` will be displayed instead
  // of dismissing the menu.
  GetReadWriteCardsUiController()
      .pre_target_handler()
      .set_dismiss_anchor_menu_on_view_closed(false);

  OnUserConsentResult(true);
}

void QuickAnswersUiController::OnUserConsentResult(bool consented) {
  DCHECK(IsShowingUserConsentView());
  controller_->OnUserConsentResult(consented);

  if (consented && IsShowingQuickAnswersView()) {
    quick_answers_view()->RequestFocus();
  }
}

bool QuickAnswersUiController::IsShowingUserConsentView() const {
  if (user_consent_view_) {
    CHECK_EQ(controller_->GetQuickAnswersVisibility(),
             QuickAnswersVisibility::kUserConsentVisible);
    return true;
  }

  return false;
}

bool QuickAnswersUiController::IsShowingQuickAnswersView() const {
  if (quick_answers_view_) {
    CHECK_EQ(controller_->GetQuickAnswersVisibility(),
             QuickAnswersVisibility::kQuickAnswersVisible);
    return true;
  }

  return false;
}

bool QuickAnswersUiController::IsShowingRichAnswersView() const {
  return rich_answers_widget_ && !rich_answers_widget_->IsClosed() &&
         rich_answers_widget_->GetContentsView();
}

chromeos::ReadWriteCardsUiController&
QuickAnswersUiController::GetReadWriteCardsUiController() const {
  return controller_->read_write_cards_ui_controller();
}
