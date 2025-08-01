// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_managed_status_finder.h"
#include "components/signin/public/identity_manager/account_managed_status_finder_outcome.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_utils.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/user_education_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kHelpCenterUrl[] =
    "https://support.google.com/chrome/answer/185277";

constexpr char kDialogCloseReasonHistogram[] =
    "Signin.DiceMigrationDialog.CloseReason";
constexpr char kDialogTimerStartedHistogram[] =
    "Signin.DiceMigrationDialog.TimerStarted";
constexpr char kDialogPreviouslyShownCountHistogram[] =
    "Signin.DiceMigrationDialog.PreviouslyShownCount";
constexpr char kDialogDaysSinceLastShownHistogram[] =
    "Signin.DiceMigrationDialog.DaysSinceLastShown";
constexpr char kDialogShownHistogram[] = "Signin.DiceMigrationDialog.Shown";
constexpr char kAccountManagedStatusHistogram[] =
    "Signin.DiceMigrationDialog.AccountManagedStatus";
constexpr char kUserMigratedHistogram[] = "Signin.DiceMigrationDialog.Migrated";
constexpr char kToastTriggeredHistogram[] =
    "Signin.DiceMigrationDialog.ToastTriggered";
constexpr char kDialogNotShownReasonHistogram[] =
    "Signin.DiceMigrationDialog.NotShownReason";

void LogDialogCloseReason(DiceMigrationService::DialogCloseReason reason) {
  base::UmaHistogramEnumeration(kDialogCloseReasonHistogram, reason);
}

void LogDialogNotShownReason(
    DiceMigrationService::DialogNotShownReason reason) {
  base::UmaHistogramEnumeration(kDialogNotShownReasonHistogram, reason);
}

void OnHelpCenterLinkClicked(Browser* browser) {
  browser->OpenGURL(GURL(kHelpCenterUrl),
                    WindowOpenDisposition::NEW_FOREGROUND_TAB);
}

bool IsUserEligibleForDiceMigration(Profile* profile) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) ||
      identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSync)) {
    // The user is not signed in or has sync enabled.
    return false;
  }
  if (!signin::IsImplicitBrowserSigninOrExplicitDisabled(identity_manager,
                                                         profile->GetPrefs())) {
    // The user is not implicitly signed in.
    return false;
  }
  return true;
}

void SetBannerImage(ui::DialogModel::Builder& builder,
                    signin::IdentityManager* identity_manager) {
  CHECK(identity_manager);
  CHECK(identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  AccountInfo account_info = identity_manager->FindExtendedAccountInfo(
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  gfx::Image avatar_image =
      account_info.account_image.IsEmpty()
          // TODO(crbug.com/399838468): This is the old placeholder avatar icon.
          // Consider using `ProfileAttributesEntry::GetAvatarIcon()` instead.
          ? ui::ResourceBundle::GetSharedInstance().GetImageNamed(
                profiles::GetPlaceholderAvatarIconResourceID())
          : account_info.account_image;

  // The position and size must match the implied one in the image,
  // so these numbers are exclusively for ..._AVATAR50_X135_Y54.
  static constexpr gfx::Point kAvatarPosition{135, 54};
  static constexpr size_t kAvatarSize{50};
  builder.SetBannerImage(profiles::EmbedAvatarOntoImage(
                             IDR_MIGRATE_ADDRESS_AVATAR50_X135_Y54,
                             avatar_image, kAvatarPosition, kAvatarSize),
                         profiles::EmbedAvatarOntoImage(
                             IDR_MIGRATE_ADDRESS_AVATAR50_X135_Y54_DARK,
                             avatar_image, kAvatarPosition, kAvatarSize));
}

bool MaybeMigrateUser(Profile* profile) {
  if (!IsUserEligibleForDiceMigration(profile)) {
    return false;
  }
  PrefService* prefs = profile->GetPrefs();
  // TODO(crbug.com/399838468): Consider calling
  // `PrimaryAccountManager::ComputeExplicitBrowserSignin` upon explicit signin
  // pref change.
  prefs->SetBoolean(prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
                    true);

  prefs->SetBoolean(prefs::kExplicitBrowserSignin, true);

  return true;
}

bool MaybeShowToast(Browser* browser) {
  ToastController* const toast_controller =
      browser->browser_window_features()->toast_controller();
  if (!toast_controller) {
    return false;
  }
  toast_controller->MaybeShowToast(ToastParams(ToastId::kDiceUserMigrated));
  return true;
}

}  // namespace

const char kDiceMigrationDialogShownCount[] =
    "signin.dice_migration.dialog_shown_count";

const char kDiceMigrationDialogLastShownTime[] =
    "signin.dice_migration.dialog_last_shown_time";

// static
const int DiceMigrationService::kMaxDialogShownCount = 3;

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DiceMigrationService,
                                      kAcceptButtonElementId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(DiceMigrationService,
                                      kCancelButtonElementId);

class DiceMigrationService::AvatarButtonObserver
    : public AvatarToolbarButton::Observer {
 public:
  AvatarButtonObserver(AvatarToolbarButton* avatar_button,
                       DiceMigrationService* dice_migration_service)
      : dice_migration_service_(dice_migration_service) {
    CHECK(avatar_button);
    CHECK(dice_migration_service_);
    CHECK(dice_migration_service_->dialog_widget_);
    avatar_button_observation_.Observe(avatar_button);
  }

 private:
  // `AvatarToolbarButton::Observer`:
  void OnButtonPressed() override {
    CHECK(dice_migration_service_->dialog_widget_);
    dice_migration_service_->StopTimerOrCloseDialog(
        DialogCloseReason::kAvatarButtonClicked);
    avatar_button_observation_.Reset();
  }

  base::ScopedObservation<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_button_observation_{this};
  raw_ptr<DiceMigrationService> dice_migration_service_;
};

DiceMigrationService::DiceMigrationService(
    Profile* profile,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing)
    : profile_(profile) {
  const std::optional<DialogNotShownReason> not_shown_reason =
      ShouldStartDialogTriggerTimer();
  base::UmaHistogramBoolean(kDialogTimerStartedHistogram,
                            !not_shown_reason.has_value());
  if (not_shown_reason.has_value()) {
    LogDialogNotShownReason(not_shown_reason.value());
    return;
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  primary_account_info_ =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  identity_manager_observation_.Observe(identity_manager);

  if (task_runner_for_testing) {
    CHECK_IS_TEST();
    dialog_trigger_timer_.SetTaskRunner(std::move(task_runner_for_testing));
  }
  dialog_trigger_timer_.Start(
      FROM_HERE,
      base::RandTimeDelta(switches::kOfferMigrationToDiceUsersMinDelay.Get(),
                          switches::kOfferMigrationToDiceUsersMaxDelay.Get()),
      base::BindOnce(
          &DiceMigrationService::OnTimerFinishOrAccountManagedStatusKnown,
          base::Unretained(this)));

  account_managed_status_finder_ =
      std::make_unique<signin::AccountManagedStatusFinder>(
          identity_manager, primary_account_info_,
          base::BindOnce(
              &DiceMigrationService::OnTimerFinishOrAccountManagedStatusKnown,
              base::Unretained(this)));
}

DiceMigrationService::~DiceMigrationService() {
  // Most likely a no-op since the dialog gets closed before this during browser
  // shutdown.
  StopTimerOrCloseDialog(DialogCloseReason::kServiceDestroyed);
}

// static
void DiceMigrationService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(kDiceMigrationDialogShownCount, 0);
  registry->RegisterTimePref(kDiceMigrationDialogLastShownTime, base::Time());
}

std::optional<DiceMigrationService::DialogNotShownReason>
DiceMigrationService::ShouldStartDialogTriggerTimer() {
  if (!IsUserEligibleForDiceMigration(profile_)) {
    return DiceMigrationService::DialogNotShownReason::kNotEligible;
  }
  const int dialog_shown_count = GetDialogShownCount();
  base::UmaHistogramExactLinear(kDialogPreviouslyShownCountHistogram,
                                dialog_shown_count, kMaxDialogShownCount + 1);
  // Show the dialog at most `kMaxDialogShownCount` times.
  if (dialog_shown_count >= kMaxDialogShownCount) {
    return DiceMigrationService::DialogNotShownReason::kMaxShownCountReached;
  }

  if (const base::Time last_shown_time = GetDialogLastShownTime();
      !last_shown_time.is_null()) {
    const base::TimeDelta duration_since_last_shown =
        base::Time::Now() - last_shown_time;
    base::UmaHistogramCounts100(kDialogDaysSinceLastShownHistogram,
                                duration_since_last_shown.InDays());
    // Show the dialog at least one week after the last time it was shown.
    if (duration_since_last_shown <
        switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get()) {
      return DiceMigrationService::DialogNotShownReason::
          kMinTimeBetweenDialogsNotPassed;
    }
  }
  return std::nullopt;
}

std::optional<DiceMigrationService::DialogNotShownReason>
DiceMigrationService::ShowDiceMigrationOfferDialogIfUserEligible() {
  CHECK(!dialog_trigger_timer_.IsRunning());
  CHECK(!dialog_widget_);
  CHECK_LT(GetDialogShownCount(), kMaxDialogShownCount);
  CHECK_LT(GetDialogLastShownTime(),
           base::Time::Now() -
               switches::kOfferMigrationToDiceUsersMinTimeBetweenDialogs.Get());

  if (!IsUserEligibleForDiceMigration(profile_)) {
    return DiceMigrationService::DialogNotShownReason::kNotEligible;
  }

  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  if (!browser || !browser->window()) {
    return DiceMigrationService::DialogNotShownReason::
        kBrowserInstanceUnavailable;
  }

  ui::DialogModelLabel::TextReplacement learn_more_link =
      ui::DialogModelLabel::CreateLink(
          IDS_LEARN_MORE,
          base::BindRepeating(&OnHelpCenterLinkClicked, browser));

  auto description_text = ui::DialogModelLabel::CreateWithReplacement(
      IDS_DICE_MIGRATION_DIALOG_DESCRIPTION, learn_more_link);

  auto builder =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>());
  SetBannerImage(builder, IdentityManagerFactory::GetForProfile(profile_));
  builder.SetTitle(l10n_util::GetStringUTF16(IDS_DICE_MIGRATION_DIALOG_TITLE));
  builder.AddParagraph(description_text);
  builder.AddOkButton(base::DoNothing(),
                      ui::DialogModel::Button::Params()
                          .SetId(kAcceptButtonElementId)
                          .SetLabel(l10n_util::GetStringUTF16(
                              IDS_DICE_MIGRATION_DIALOG_OK_BUTTON)));

  // The "final" variant does not include a close button, but rather the close-x
  // button.
  if (GetDialogShownCount() < kMaxDialogShownCount - 1) {
    // Non-"final" variant.
    builder.OverrideShowCloseButton(false);
    builder.AddCancelButton(
        base::DoNothing(),
        ui::DialogModel::Button::Params()
            .SetId(kCancelButtonElementId)
            .SetLabel(l10n_util::GetStringUTF16(IDS_NOT_NOW)));
  }

  // TODO(crbug.com/399838468): Refine the dialog behavior.
  builder.DisableCloseOnDeactivate();
  builder.SetIsAlertDialog();

  AvatarToolbarButton* avatar_button =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetAvatarToolbarButton();
  if (!avatar_button) {
    // Skip showing the dialog if the avatar button is not available.
    return DiceMigrationService::DialogNotShownReason::kAvatarButtonUnavailable;
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      builder.Build(), avatar_button, views::BubbleBorder::TOP_RIGHT);
  dialog_widget_ = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  dialog_widget_observation_.Observe(dialog_widget_);
  browser_ = browser->AsWeakPtr();
  dialog_widget_->Show();

  // Close the dialog when the avatar pill is clicked.
  avatar_button_observer_ =
      std::make_unique<AvatarButtonObserver>(avatar_button, this);

  return std::nullopt;
}

views::Widget* DiceMigrationService::GetDialogWidgetForTesting() {
  return dialog_widget_.get();
}

base::OneShotTimer& DiceMigrationService::GetDialogTriggerTimerForTesting() {
  return dialog_trigger_timer_;
}

void DiceMigrationService::OnWidgetDestroying(views::Widget* widget) {
  CHECK_EQ(dialog_widget_, widget);
  avatar_button_observer_.reset();
  dialog_widget_observation_.Reset();
  dialog_widget_ = nullptr;
  Browser* browser = browser_.get();
  browser_.reset();
  switch (widget->closed_reason()) {
    // Losing focus should not close the dialog.
    case views::Widget::ClosedReason::kLostFocus:
      NOTREACHED();
    case views::Widget::ClosedReason::kUnspecified:
      LogDialogCloseReason(
          dialog_close_reason_.value_or(DialogCloseReason::kUnspecified));
      return;
    case views::Widget::ClosedReason::kAcceptButtonClicked: {
      LogDialogCloseReason(DialogCloseReason::kAccepted);
      const bool migrated = MaybeMigrateUser(profile_);
      base::UmaHistogramBoolean(kUserMigratedHistogram, migrated);
      if (migrated) {
        const bool toast_triggered = browser && MaybeShowToast(browser);
        base::UmaHistogramBoolean(kToastTriggeredHistogram, toast_triggered);
      }
    } break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      // Cancel button is only available in the non-"final" variant.
      CHECK_LT(GetDialogShownCount(), kMaxDialogShownCount - 1);
      LogDialogCloseReason(DialogCloseReason::kCancelled);
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      // Close button is only available in the "final" variant.
      CHECK_EQ(GetDialogShownCount(), kMaxDialogShownCount - 1);
      LogDialogCloseReason(DialogCloseReason::kClosed);
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
      LogDialogCloseReason(DialogCloseReason::kEscKeyPressed);
      break;
  }
  // The dialog is considered shown if the user interacts with it, i.e. the user
  // accepts or dismisses the dialog. This is better than just tracking when the
  // dialog was actually shown, since the user might have dismissed the dialog
  // unknowingly, for example, by closing the browser.
  UpdateDialogShownCountAndTime();
}

void DiceMigrationService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      CHECK_EQ(primary_account_info_, event.GetPreviousState().primary_account);
      StopTimerOrCloseDialog(DialogCloseReason::kPrimaryAccountChanged);
      break;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      CHECK_EQ(primary_account_info_, event.GetPreviousState().primary_account);
      StopTimerOrCloseDialog(DialogCloseReason::kPrimaryAccountCleared);
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      CHECK_EQ(primary_account_info_, event.GetCurrentState().primary_account);
      break;
  }
}

void DiceMigrationService::OnTimerFinishOrAccountManagedStatusKnown() {
  if (dialog_trigger_timer_.IsRunning()) {
    return;
  }
  signin::AccountManagedStatusFinderOutcome
      account_managed_status_finder_outcome =
          account_managed_status_finder_->GetOutcome();
  base::UmaHistogramEnumeration(kAccountManagedStatusHistogram,
                                account_managed_status_finder_outcome);
  switch (account_managed_status_finder_outcome) {
    case signin::AccountManagedStatusFinderOutcome::kPending:
      return;
    case signin::AccountManagedStatusFinderOutcome::kError:
    case signin::AccountManagedStatusFinderOutcome::kTimeout:
      LogDialogNotShownReason(DiceMigrationService::DialogNotShownReason::
                                  kErrorFetchingAccountManagedStatus);
      return;
    // Consumer accounts.
    case signin::AccountManagedStatusFinderOutcome::kConsumerGmail:
    case signin::AccountManagedStatusFinderOutcome::kConsumerWellKnown:
    case signin::AccountManagedStatusFinderOutcome::kConsumerNotWellKnown: {
      const std::optional<DialogNotShownReason> not_shown_reason =
          ShowDiceMigrationOfferDialogIfUserEligible();
      base::UmaHistogramBoolean(kDialogShownHistogram,
                                !not_shown_reason.has_value());
      if (not_shown_reason.has_value()) {
        LogDialogNotShownReason(not_shown_reason.value());
      }
    } break;
    // Managed accounts are not shown the migration dialog.
    case signin::AccountManagedStatusFinderOutcome::kEnterpriseGoogleDotCom:
    case signin::AccountManagedStatusFinderOutcome::kEnterprise:
      LogDialogNotShownReason(
          DiceMigrationService::DialogNotShownReason::kManagedAccount);
      return;
  }
}

void DiceMigrationService::StopTimerOrCloseDialog(
    DiceMigrationService::DialogCloseReason reason) {
  CHECK(!dialog_trigger_timer_.IsRunning() || !dialog_widget_);
  identity_manager_observation_.Reset();
  if (dialog_widget_) {
    dialog_close_reason_ = reason;
    dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  } else if (dialog_trigger_timer_.IsRunning()) {
    dialog_trigger_timer_.Stop();
    switch (reason) {
      case DialogCloseReason::kPrimaryAccountChanged:
        LogDialogNotShownReason(
            DiceMigrationService::DialogNotShownReason::kPrimaryAccountChanged);
        break;
      case DialogCloseReason::kPrimaryAccountCleared:
        LogDialogNotShownReason(
            DiceMigrationService::DialogNotShownReason::kPrimaryAccountCleared);
        break;
      case DialogCloseReason::kServiceDestroyed:
        LogDialogNotShownReason(
            DiceMigrationService::DialogNotShownReason::kServiceDestroyed);
        break;
      default:
        NOTREACHED();
    }
  }
}

int DiceMigrationService::GetDialogShownCount() const {
  PrefService* prefs = profile_->GetPrefs();
  CHECK(prefs);
  return prefs->GetInteger(kDiceMigrationDialogShownCount);
}

base::Time DiceMigrationService::GetDialogLastShownTime() const {
  PrefService* prefs = profile_->GetPrefs();
  CHECK(prefs);
  return prefs->GetTime(kDiceMigrationDialogLastShownTime);
}

void DiceMigrationService::UpdateDialogShownCountAndTime() {
  PrefService* prefs = profile_->GetPrefs();
  CHECK(prefs);
  prefs->SetInteger(kDiceMigrationDialogShownCount, GetDialogShownCount() + 1);
  prefs->SetTime(kDiceMigrationDialogLastShownTime, base::Time::Now());
}
