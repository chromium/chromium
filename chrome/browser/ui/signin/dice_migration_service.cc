// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/dice_migration_service.h"

#include "base/check_is_test.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
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
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_education/common/user_education_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char kHelpCenterUrl[] =
    "https://support.google.com/chrome/answer/"
    "185277?p=manage_autofill_settings";

constexpr base::TimeDelta kForcedSigninToastDelay = base::Seconds(5);

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
constexpr char kRestoredFromBackupHistogram[] =
    "Signin.DiceMigration.RestoredFromBackup";
constexpr char kForceMigratedHistogram[] = "Signin.DiceMigration.ForceMigrated";
constexpr char kForcedMigrationAccountManagedHistogram[] =
    "Signin.ForcedDiceMigration.HasAcceptedAccountManagement";
constexpr char kForcedSigninBrowserInstanceAvailableAfterTimerHistogram[] =
    "Signin.ForcedDiceMigration.BrowserInstanceAvailableAfterTimer";

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
  CHECK(prefs);

  // Backup the prefs.
  prefs->SetDict(
      kDiceMigrationBackup,
      base::Value::Dict()
          .SetByDottedPath(prefs::kExplicitBrowserSignin,
                           prefs->GetBoolean(prefs::kExplicitBrowserSignin))
          .SetByDottedPath(
              prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
              prefs->GetBoolean(
                  prefs::kPrefsThemesSearchEnginesAccountStorageEnabled)));

  // TODO(crbug.com/399838468): Consider calling
  // `PrimaryAccountManager::SetExplicitBrowserSigninPrefs` upon explicit signin
  // pref change.
  prefs->SetBoolean(prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
                    true);

  prefs->SetBoolean(prefs::kExplicitBrowserSignin, true);

  // Mark the migration pref as successful.
  prefs->SetBoolean(kDiceMigrationMigrated, true);
  // Reset the restoration pref.
  prefs->SetBoolean(kDiceMigrationRestoredFromBackup, false);
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

const char kDiceMigrationMigrated[] = "signin.dice_migration.migrated";

const char kDiceMigrationBackup[] = "signin.dice_migration.backup";

const char kDiceMigrationRestoredFromBackup[] =
    "signin.dice_migration.restored_from_backup";

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

    const std::u16string& avatar_button_text = base::ASCIIToUTF16(
        dice_migration_service_->primary_account_info_.email);
    if (!avatar_button_text.empty()) {
      clear_avatar_button_effects_callback_ =
          avatar_button->SetExplicitButtonState(
              avatar_button_text, /*accessibility_label=*/std::nullopt,
              /*explicit_action=*/std::nullopt);
    }
  }

 private:
  // `AvatarToolbarButton::Observer`:
  void OnButtonPressed() override {
    CHECK(dice_migration_service_->dialog_widget_);
    avatar_button_observation_.Reset();
    dice_migration_service_->StopTimerOrCloseDialog(
        DialogCloseReason::kAvatarButtonClicked);
  }

  base::ScopedObservation<AvatarToolbarButton, AvatarToolbarButton::Observer>
      avatar_button_observation_{this};
  raw_ptr<DiceMigrationService> dice_migration_service_;

  // Callback to reset the expanded avatar button.
  base::ScopedClosureRunner clear_avatar_button_effects_callback_;
};

DiceMigrationService::DiceMigrationService(
    Profile* profile,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_testing)
    : profile_(profile) {
  CHECK(profile_);
  if (base::FeatureList::IsEnabled(switches::kForcedDiceMigration)) {
    // Force migration all implicitly signed-in users.
    const bool migrated = ForceMigrateUserIfEligible();
    base::UmaHistogramBoolean(kForceMigratedHistogram, migrated);
    // By now, the user should have been force migrated and is no longer in the
    // DICe state.
    CHECK(!IsUserEligibleForDiceMigration(profile_));
    return;
  }

  CHECK(base::FeatureList::IsEnabled(switches::kOfferMigrationToDiceUsers));
  const std::optional<DialogNotShownReason> not_shown_reason =
      ShouldStartDialogTriggerTimer();
  base::UmaHistogramBoolean(kDialogTimerStartedHistogram,
                            !not_shown_reason.has_value());
  if (not_shown_reason.has_value()) {
    LogDialogNotShownReason(not_shown_reason.value());
    return;
  }
  // If the flag is enabled, the user should have already been migrated and
  // the above return statement should have returned.
  CHECK(!base::FeatureList::IsEnabled(switches::kForcedDiceMigration));

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
  registry->RegisterBooleanPref(kDiceMigrationMigrated, false);
  registry->RegisterDictionaryPref(kDiceMigrationBackup);
  registry->RegisterBooleanPref(kDiceMigrationRestoredFromBackup, false);
}

// static
void DiceMigrationService::RevertDiceMigration(PrefService* prefs) {
  CHECK(prefs);

  if (!prefs->GetBoolean(kDiceMigrationMigrated)) {
    return;
  }

  const bool restored_from_backup = [prefs]() -> bool {
    const base::Value* backup = prefs->GetUserPrefValue(kDiceMigrationBackup);
    if (!backup || !backup->is_dict()) {
      return false;
    }
    const std::optional<bool> prefs_account_storage_enabled =
        backup->GetDict().FindBoolByDottedPath(
            prefs::kPrefsThemesSearchEnginesAccountStorageEnabled);
    const std::optional<bool> explicit_browser_signin =
        backup->GetDict().FindBoolByDottedPath(prefs::kExplicitBrowserSignin);
    if (!explicit_browser_signin.has_value() ||
        !prefs_account_storage_enabled.has_value()) {
      return false;
    }
    prefs->SetBoolean(prefs::kPrefsThemesSearchEnginesAccountStorageEnabled,
                      *prefs_account_storage_enabled);
    prefs->SetBoolean(prefs::kExplicitBrowserSignin, *explicit_browser_signin);
    return true;
  }();

  prefs->SetBoolean(kDiceMigrationRestoredFromBackup, restored_from_backup);
  prefs->SetBoolean(kDiceMigrationMigrated, !restored_from_backup);
  base::UmaHistogramBoolean(kRestoredFromBackupHistogram, restored_from_backup);
  // Clear the backup. Also clear the dialog shown count/time to ensure the
  // dialog can be shown again once the flag is enabled again.
  if (restored_from_backup) {
    prefs->ClearPref(kDiceMigrationBackup);
    prefs->ClearPref(kDiceMigrationDialogShownCount);
  }
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

  AvatarToolbarButton* avatar_button =
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetAvatarToolbarButton();
  // Skip showing the dialog if the avatar button is not available.
  // Some browsers, such as web apps, don't have an avatar toolbar button to
  // anchor the bubble. Even if a web app has an avatar toolbar button, avoid
  // showing the dialog to keep the behavior consistent.
  if (!avatar_button || web_app::AppBrowserController::IsWebApp(browser)) {
    return DiceMigrationService::DialogNotShownReason::kAvatarButtonUnavailable;
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
            .SetLabel(l10n_util::GetStringUTF16(IDS_PROMO_SNOOZE_BUTTON)));
  }
  builder.DisableCloseOnDeactivate();
  builder.SetIsAlertDialog();

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      builder.Build(), avatar_button, views::BubbleBorder::TOP_RIGHT);
  dialog_widget_ = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  dialog_widget_observation_.Observe(dialog_widget_);

  browser_ = browser->AsWeakPtr();
  browser_close_subscription_ =
      browser->RegisterBrowserDidClose(base::BindRepeating(
          &DiceMigrationService::BrowserDidClose, base::Unretained(this)));

  dialog_widget_->Show();

  // Update the dialog shown count and time. Note that the user may not interact
  // with the dialog at all, for example, if they close the browser. Or, the
  // dialog may be shown on a browser that is minimized. These cases still count
  // as showing the dialog. This is better than the alternate of updating the
  // shown count and time only when the user interacts with the dialog, which
  // might cause the dialog to be show on every browser startup if the user
  // never interacts with it.
  UpdateDialogShownCountAndTime();

  // Close the dialog when the avatar pill is clicked. This is also responsible
  // for expanding the avatar pill when the dialog is showing.
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
  browser_close_subscription_.reset();
  dialog_widget_ = nullptr;
  switch (widget->closed_reason()) {
    // Losing focus should not close the dialog.
    case views::Widget::ClosedReason::kLostFocus:
      NOTREACHED();
    case views::Widget::ClosedReason::kUnspecified:
      LogDialogCloseReason(
          dialog_close_reason_.value_or(DialogCloseReason::kUnspecified));
      break;
    case views::Widget::ClosedReason::kAcceptButtonClicked: {
      LogDialogCloseReason(DialogCloseReason::kAccepted);
      const bool migrated = MaybeMigrateUser(profile_);
      base::UmaHistogramBoolean(kUserMigratedHistogram, migrated);
      if (migrated) {
        const bool toast_triggered = browser_ && MaybeShowToast(browser_.get());
        base::UmaHistogramBoolean(kToastTriggeredHistogram, toast_triggered);
      }
    } break;
    case views::Widget::ClosedReason::kCancelButtonClicked:
      // Cancel button is only available in the non-"final" variant.
      CHECK_LT(GetDialogShownCount(), kMaxDialogShownCount);
      LogDialogCloseReason(DialogCloseReason::kCancelled);
      break;
    case views::Widget::ClosedReason::kCloseButtonClicked:
      // Close button is only available in the "final" variant.
      CHECK_EQ(GetDialogShownCount(), kMaxDialogShownCount);
      LogDialogCloseReason(DialogCloseReason::kClosed);
      break;
    case views::Widget::ClosedReason::kEscKeyPressed:
      LogDialogCloseReason(DialogCloseReason::kEscKeyPressed);
      break;
  }
  browser_.reset();
}

void DiceMigrationService::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event) {
  switch (event.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
      CHECK_EQ(primary_account_info_, event.GetPreviousState().primary_account);
      StopTimerOrCloseDialog(DialogCloseReason::kPrimaryAccountChanged);
      return;
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      CHECK_EQ(primary_account_info_, event.GetPreviousState().primary_account);
      StopTimerOrCloseDialog(DialogCloseReason::kPrimaryAccountCleared);
      return;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      CHECK_EQ(primary_account_info_, event.GetCurrentState().primary_account);
      break;
  }
  // If the user turns sync on, stop the timer or close the dialog.
  if (event.GetEventTypeFor(signin::ConsentLevel::kSync) ==
      signin::PrimaryAccountChangeEvent::Type::kSet) {
    StopTimerOrCloseDialog(DialogCloseReason::kSyncTurnedOn);
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
      case DialogCloseReason::kSyncTurnedOn:
        LogDialogNotShownReason(
            DiceMigrationService::DialogNotShownReason::kSyncTurnedOn);
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

void DiceMigrationService::BrowserDidClose(BrowserWindowInterface* browser) {
  if (dialog_widget_) {
    dialog_widget_->CloseNow();
  }
}

bool DiceMigrationService::ForceMigrateUserIfEligible() {
  if (!IsUserEligibleForDiceMigration(profile_)) {
    return false;
  }
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  CHECK(identity_manager);
  const bool has_accepted_account_management =
      enterprise_util::UserAcceptedAccountManagement(profile_);
  base::UmaHistogramBoolean(kForcedMigrationAccountManagedHistogram,
                            has_accepted_account_management);
  if (!has_accepted_account_management) {
    // This is either a consumer account or an enterprise account that has not
    // accepted the account management.
    // Remove the primary account, but keep the tokens. This will make the user
    // signed in only to the web.
    CHECK(identity_manager->GetPrimaryAccountMutator()
              ->RemovePrimaryAccountButKeepTokens(
                  signin_metrics::ProfileSignout::kForcedDiceMigration));
    return true;
  }
  // The user is an enterprise account that has accepted the account management.
  // Such users cannot be signed out. Migrate these users to explicitly
  // signed-in state.
  CHECK(MaybeMigrateUser(profile_));

  // Trigger the timer to show the toast.
  auto show_toast = [](Profile* profile) {
    Browser* browser = chrome::FindBrowserWithProfile(profile);
    base::UmaHistogramBoolean(
        kForcedSigninBrowserInstanceAvailableAfterTimerHistogram, browser);
    if (browser) {
      CHECK(MaybeShowToast(browser));
    } else {
      // The profile is under creation and hence no browser instance is tied to
      // it yet. Wait for the browser to be created before trying to show the
      // toast.
      // This object deletes itself when done.
      new profiles::BrowserAddedForProfileObserver(
          profile, base::BindOnce(base::IgnoreResult(&MaybeShowToast)));
    }
  };
  // TODO(crbug.com/437083916): Rename the timer to reflect that it is also used
  // for showing this toast.
  CHECK(!dialog_trigger_timer_.IsRunning());
  dialog_trigger_timer_.Start(FROM_HERE, kForcedSigninToastDelay,
                              base::BindOnce(std::move(show_toast), profile_));
  return true;
}
