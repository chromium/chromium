// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"

#include <cmath>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/arc/arc_migration_constants.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/ui/login_feedback.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "chromeos/dbus/power_manager/power_supply_properties.pb.h"
#include "components/login/localized_values_builder.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/system_connector.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/wake_lock_provider.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/text/bytes_formatting.h"
#include "ui/chromeos/devicetype_utils.h"

namespace {

// Path to the mount point to check the available space.
constexpr char kCheckStoragePath[] = "/home";

// JS API callbacks names.
constexpr char kJsApiStartMigration[] = "startMigration";
constexpr char kJsApiSkipMigration[] = "skipMigration";
constexpr char kJsApiRequestRestartOnLowStorage[] =
    "requestRestartOnLowStorage";
constexpr char kJsApiRequestRestartOnFailure[] = "requestRestartOnFailure";
constexpr char kJsApiOpenFeedbackDialog[] = "openFeedbackDialog";

// If minimal migration takes this threshold or longer (in seconds), we
// will ask the user to re-enter their password.
constexpr int64_t kMinimalMigrationReenterPasswordThreshold = 45;

// UMA names.
constexpr char kUmaNameFirstScreen[] = "Cryptohome.MigrationUI.FirstScreen";
constexpr char kUmaNameUserChoice[] = "Cryptohome.MigrationUI.UserChoice";
constexpr char kUmaNameMigrationResult[] =
    "Cryptohome.MigrationUI.MigrationResult";
constexpr char kUmaNameRemoveCryptohomeResult[] =
    "Cryptohome.MigrationUI.RemoveCryptohomeResult";
constexpr char kUmaNameConsumedBatteryPercent[] =
    "Cryptohome.MigrationUI.ConsumedBatteryPercent";
constexpr char kUmaNameVisibleScreen[] = "Cryptohome.MigrationUI.VisibleScreen";

// This enum must match the numbering for MigrationUIFirstScreen in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before FIRST_SCREEN_COUNT.
enum class FirstScreen {
  FIRST_SCREEN_READY = 0,
  FIRST_SCREEN_RESUME = 1,
  FIRST_SCREEN_LOW_STORAGE = 2,
  FIRST_SCREEN_ARC_KIOSK = 3,
  FIRST_SCREEN_START_AUTOMATICALLY = 4,
  FIRST_SCREEN_RESUME_MINIMAL = 5,
  FIRST_SCREEN_START_AUTOMATICALLY_MINIMAL = 6,
  FIRST_SCREEN_COUNT
};

// This enum must match the numbering for MigrationUIUserChoice in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before USER_CHOICE_COUNT.
enum class UserChoice {
  USER_CHOICE_UPDATE = 0,
  USER_CHOICE_SKIP = 1,
  USER_CHOICE_RESTART_ON_FAILURE = 2,
  USER_CHOICE_RESTART_ON_LOW_STORAGE = 3,
  USER_CHOICE_REPORT_AN_ISSUE = 4,
  USER_CHOICE_COUNT
};

// This enum must match the numbering for MigrationUIMigrationResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before COUNT.
enum class MigrationResult {
  SUCCESS_IN_NEW_MIGRATION = 0,
  SUCCESS_IN_RESUMED_MIGRATION = 1,
  GENERAL_FAILURE_IN_NEW_MIGRATION = 2,
  GENERAL_FAILURE_IN_RESUMED_MIGRATION = 3,
  REQUEST_FAILURE_IN_NEW_MIGRATION = 4,
  REQUEST_FAILURE_IN_RESUMED_MIGRATION = 5,
  MOUNT_FAILURE_IN_NEW_MIGRATION = 6,
  MOUNT_FAILURE_IN_RESUMED_MIGRATION = 7,
  SUCCESS_IN_ARC_KIOSK_MIGRATION = 8,
  GENERAL_FAILURE_IN_ARC_KIOSK_MIGRATION = 9,
  REQUEST_FAILURE_IN_ARC_KIOSK_MIGRATION = 10,
  MOUNT_FAILURE_IN_ARC_KIOSK_MIGRATION = 11,
  COUNT
};

// This enum must match the numbering for MigrationUIRemoveCryptohomeResult in
// histograms/enums.xml. Do not reorder or remove items, only add new items
// before COUNT.
enum class RemoveCryptohomeResult {
  SUCCESS_IN_NEW_MIGRATION = 0,
  SUCCESS_IN_RESUMED_MIGRATION = 1,
  FAILURE_IN_NEW_MIGRATION = 2,
  FAILURE_IN_RESUMED_MIGRATION = 3,
  SUCCESS_IN_ARC_KIOSK_MIGRATION = 4,
  FAILURE_IN_ARC_KIOSK_MIGRATION = 5,
  COUNT
};

bool IsTestingUI() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kTestEncryptionMigrationUI);
}

// Wrapper functions for histogram macros to avoid duplication of expanded code.
void RecordFirstScreen(FirstScreen first_screen) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameFirstScreen, first_screen,
                            FirstScreen::FIRST_SCREEN_COUNT);
}

void RecordUserChoice(UserChoice user_choice) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameUserChoice, user_choice,
                            UserChoice::USER_CHOICE_COUNT);
}

void RecordMigrationResult(MigrationResult migration_result) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameMigrationResult, migration_result,
                            MigrationResult::COUNT);
}

void RecordMigrationResultSuccess(bool resume, bool arc_kiosk) {
  if (arc_kiosk)
    RecordMigrationResult(MigrationResult::SUCCESS_IN_ARC_KIOSK_MIGRATION);
  else if (resume)
    RecordMigrationResult(MigrationResult::SUCCESS_IN_RESUMED_MIGRATION);
  else
    RecordMigrationResult(MigrationResult::SUCCESS_IN_NEW_MIGRATION);
}

void RecordMigrationResultGeneralFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordMigrationResult(
        MigrationResult::GENERAL_FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordMigrationResult(
        MigrationResult::GENERAL_FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordMigrationResult(MigrationResult::GENERAL_FAILURE_IN_NEW_MIGRATION);
  }
}

void RecordMigrationResultRequestFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordMigrationResult(
        MigrationResult::REQUEST_FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordMigrationResult(
        MigrationResult::REQUEST_FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordMigrationResult(MigrationResult::REQUEST_FAILURE_IN_NEW_MIGRATION);
  }
}

void RecordMigrationResultMountFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordMigrationResult(
        MigrationResult::MOUNT_FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordMigrationResult(MigrationResult::MOUNT_FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordMigrationResult(MigrationResult::MOUNT_FAILURE_IN_NEW_MIGRATION);
  }
}

void RecordRemoveCryptohomeResult(RemoveCryptohomeResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUmaNameRemoveCryptohomeResult, result,
                            RemoveCryptohomeResult::COUNT);
}

void RecordRemoveCryptohomeResultSuccess(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::SUCCESS_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::SUCCESS_IN_RESUMED_MIGRATION);
  } else {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::SUCCESS_IN_NEW_MIGRATION);
  }
}

void RecordRemoveCryptohomeResultFailure(bool resume, bool arc_kiosk) {
  if (arc_kiosk) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::FAILURE_IN_ARC_KIOSK_MIGRATION);
  } else if (resume) {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::FAILURE_IN_RESUMED_MIGRATION);
  } else {
    RecordRemoveCryptohomeResult(
        RemoveCryptohomeResult::FAILURE_IN_NEW_MIGRATION);
  }
}

// Chooses the value for the MigrationUIFirstScreen UMA stat. Not used for ARC
// kiosk.
FirstScreen GetFirstScreenForMode(chromeos::EncryptionMigrationMode mode) {
  switch (mode) {
    case chromeos::EncryptionMigrationMode::ASK_USER:
      return FirstScreen::FIRST_SCREEN_READY;
    case chromeos::EncryptionMigrationMode::START_MIGRATION:
      return FirstScreen::FIRST_SCREEN_START_AUTOMATICALLY;
    case chromeos::EncryptionMigrationMode::START_MINIMAL_MIGRATION:
      return FirstScreen::FIRST_SCREEN_START_AUTOMATICALLY_MINIMAL;
    case chromeos::EncryptionMigrationMode::RESUME_MIGRATION:
      return FirstScreen::FIRST_SCREEN_RESUME;
    case chromeos::EncryptionMigrationMode::RESUME_MINIMAL_MIGRATION:
      return FirstScreen::FIRST_SCREEN_RESUME_MINIMAL;
    default:
      NOTREACHED();
  }
}

}  // namespace

namespace chromeos {

constexpr StaticOobeScreenId EncryptionMigrationScreenView::kScreenId;

EncryptionMigrationScreenHandler::EncryptionMigrationScreenHandler(
    JSCallsContainer* js_calls_container)
    : BaseScreenHandler(kScreenId, js_calls_container),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  free_disk_space_fetcher_ = base::Bind(&base::SysInfo::AmountOfFreeDiskSpace,
                                        base::FilePath(kCheckStoragePath));
}

EncryptionMigrationScreenHandler::~EncryptionMigrationScreenHandler() {
  CryptohomeClient::Get()->RemoveObserver(this);
  PowerManagerClient::Get()->RemoveObserver(this);
  if (delegate_)
    delegate_->OnViewDestroyed(this);
}

void EncryptionMigrationScreenHandler::Show() {
  if (!page_is_ready() || !delegate_) {
    show_on_init_ = true;
    return;
  }
  ShowScreen(kScreenId);
}

void EncryptionMigrationScreenHandler::Hide() {
  show_on_init_ = false;
}

void EncryptionMigrationScreenHandler::SetDelegate(
    EncryptionMigrationScreen* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void EncryptionMigrationScreenHandler::SetUserContext(
    const UserContext& user_context) {
  user_context_ = user_context;
}

void EncryptionMigrationScreenHandler::SetMode(EncryptionMigrationMode mode) {
  mode_ = mode;
  CallJS("login.EncryptionMigrationScreen.setIsResuming", IsStartImmediately());
}

void EncryptionMigrationScreenHandler::SetContinueLoginCallback(
    ContinueLoginCallback callback) {
  continue_login_callback_ = std::move(callback);
}

void EncryptionMigrationScreenHandler::SetRestartLoginCallback(
    RestartLoginCallback callback) {
  restart_login_callback_ = std::move(callback);
}

void EncryptionMigrationScreenHandler::SetupInitialView() {
  // Pass constant value(s) to the UI.
  CallJS("login.EncryptionMigrationScreen.setNecessaryBatteryPercent",
         arc::kMigrationMinimumBatteryPercent);

  // If old encryption is detected in ARC kiosk mode, skip all checks (user
  // confirmation, battery level, and remaining space) and start migration
  // immediately.
  if (IsArcKiosk()) {
    RecordFirstScreen(FirstScreen::FIRST_SCREEN_ARC_KIOSK);
    StartMigration();
    return;
  }
  PowerManagerClient::Get()->AddObserver(this);
  CheckAvailableStorage();
}

void EncryptionMigrationScreenHandler::DeclareLocalizedValues(
    ::login::LocalizedValuesBuilder* builder) {
  builder->Add("migrationReadyTitle", IDS_ENCRYPTION_MIGRATION_READY_TITLE);
  builder->Add("migrationReadyDescription",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_READY_DESCRIPTION));
  builder->Add("migrationMigratingTitle",
               IDS_ENCRYPTION_MIGRATION_MIGRATING_TITLE);
  builder->Add("migrationMigratingDescription",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_MIGRATING_DESCRIPTION));
  builder->Add("migrationProgressLabel",
               IDS_ENCRYPTION_MIGRATION_PROGRESS_LABEL);
  builder->Add("migrationBatteryWarningLabel",
               IDS_ENCRYPTION_MIGRATION_BATTERY_WARNING_LABEL);
  builder->Add("migrationAskChargeMessage",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_ASK_CHARGE_MESSAGE));
  builder->Add("migrationNecessaryBatteryLevelLabel",
               IDS_ENCRYPTION_MIGRATION_NECESSARY_BATTERY_LEVEL_MESSAGE);
  builder->Add("migrationChargingLabel",
               IDS_ENCRYPTION_MIGRATION_CHARGING_LABEL);
  builder->Add("migrationFailedTitle", IDS_ENCRYPTION_MIGRATION_FAILED_TITLE);
  builder->Add("migrationFailedSubtitle",
               IDS_ENCRYPTION_MIGRATION_FAILED_SUBTITLE);
  builder->Add("migrationFailedMessage",
               ui::SubstituteChromeOSDeviceType(
                   IDS_ENCRYPTION_MIGRATION_FAILED_MESSAGE));
  builder->Add("migrationNospaceWarningLabel",
               IDS_ENCRYPTION_MIGRATION_NOSPACE_WARNING_LABEL);
  builder->Add("migrationAskFreeSpaceMessage",
               IDS_ENCRYPTION_MIGRATION_ASK_FREE_SPACE_MESSAGE);
  builder->Add("migrationAvailableSpaceLabel",
               IDS_ENCRYPTION_MIGRATION_AVAILABLE_SPACE_LABEL);
  builder->Add("migrationNecessarySpaceLabel",
               IDS_ENCRYPTION_MIGRATION_NECESSARY_SPACE_LABEL);
  builder->Add("migrationButtonUpdate", IDS_ENCRYPTION_MIGRATION_BUTTON_UPDATE);
  builder->Add("migrationButtonSkip", IDS_ENCRYPTION_MIGRATION_BUTTON_SKIP);
  builder->Add("migrationButtonRestart",
               IDS_ENCRYPTION_MIGRATION_BUTTON_RESTART);
  builder->Add("migrationButtonContinue",
               IDS_ENCRYPTION_MIGRATION_BUTTON_CONTINUE);
  builder->Add("migrationButtonSignIn", IDS_ENCRYPTION_MIGRATION_BUTTON_SIGNIN);
  builder->Add("migrationButtonReportAnIssue", IDS_REPORT_AN_ISSUE);
  builder->Add("migrationBoardName", base::SysInfo::GetLsbReleaseBoard());
  builder->Add("gaiaLoading", IDS_LOGIN_GAIA_LOADING_MESSAGE);
}

void EncryptionMigrationScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void EncryptionMigrationScreenHandler::SetFreeDiskSpaceFetcherForTesting(
    FreeDiskSpaceFetcher free_disk_space_fetcher) {
  free_disk_space_fetcher_ = std::move(free_disk_space_fetcher);
}

void EncryptionMigrationScreenHandler::SetTickClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void EncryptionMigrationScreenHandler::RegisterMessages() {
  AddCallback(kJsApiStartMigration,
              &EncryptionMigrationScreenHandler::HandleStartMigration);
  AddCallback(kJsApiSkipMigration,
              &EncryptionMigrationScreenHandler::HandleSkipMigration);
  AddCallback(
      kJsApiRequestRestartOnLowStorage,
      &EncryptionMigrationScreenHandler::HandleRequestRestartOnLowStorage);
  AddCallback(kJsApiRequestRestartOnFailure,
              &EncryptionMigrationScreenHandler::HandleRequestRestartOnFailure);
  AddCallback(kJsApiOpenFeedbackDialog,
              &EncryptionMigrationScreenHandler::HandleOpenFeedbackDialog);
}

void EncryptionMigrationScreenHandler::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  if (proto.has_battery_percent()) {
    if (!current_battery_percent_) {
      // If initial battery level is below the minimum, migration should start
      // automatically once the device is charged enough.
      if (proto.battery_percent() < arc::kMigrationMinimumBatteryPercent) {
        should_migrate_on_enough_battery_ = true;
        // If migration was forced by policy, stop forcing it (we don't want the
        // user to have to wait until the battery is charged).
        MaybeStopForcingMigration();
      }
    }
    current_battery_percent_ = proto.battery_percent();
  } else {
    // If battery level is not provided, we regard it as 100% to start migration
    // immediately.
    current_battery_percent_ = 100.0;
  }

  CallJS("login.EncryptionMigrationScreen.setBatteryState",
         *current_battery_percent_,
         *current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent,
         proto.battery_state() ==
             power_manager::PowerSupplyProperties_BatteryState_CHARGING);

  // If the migration was already requested and the bettery level is enough now,
  // The migration should start immediately.
  if (*current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent &&
      should_migrate_on_enough_battery_) {
    should_migrate_on_enough_battery_ = false;
    StartMigration();
  }
}

void EncryptionMigrationScreenHandler::HandleStartMigration() {
  RecordUserChoice(UserChoice::USER_CHOICE_UPDATE);
  WaitBatteryAndMigrate();
}

void EncryptionMigrationScreenHandler::HandleSkipMigration() {
  RecordUserChoice(UserChoice::USER_CHOICE_SKIP);
  // If the user skips migration, we mount the cryptohome without performing the
  // migration by reusing UserContext and LoginPerformer which were used in the
  // previous attempt and dropping |is_forcing_dircrypto| flag in UserContext.
  // In this case, the user can not launch ARC apps in the session, and will be
  // asked to do the migration again in the next log-in attempt.
  if (!continue_login_callback_.is_null()) {
    user_context_.SetIsForcingDircrypto(false);
    std::move(continue_login_callback_).Run(user_context_);
  }
}

void EncryptionMigrationScreenHandler::HandleRequestRestartOnLowStorage() {
  RecordUserChoice(UserChoice::USER_CHOICE_RESTART_ON_LOW_STORAGE);
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER,
      "login encryption migration low storage");
}

void EncryptionMigrationScreenHandler::HandleRequestRestartOnFailure() {
  RecordUserChoice(UserChoice::USER_CHOICE_RESTART_ON_FAILURE);
  PowerManagerClient::Get()->RequestRestart(
      power_manager::REQUEST_RESTART_OTHER,
      "login encryption migration failure");
}

void EncryptionMigrationScreenHandler::HandleOpenFeedbackDialog() {
  RecordUserChoice(UserChoice::USER_CHOICE_REPORT_AN_ISSUE);
  const std::string description = base::StringPrintf(
      "Auto generated feedback for http://crbug.com/719266.\n"
      "(uniquifier:%s)",
      base::NumberToString(base::Time::Now().ToInternalValue()).c_str());
  login_feedback_ =
      std::make_unique<LoginFeedback>(Profile::FromWebUI(web_ui()));
  login_feedback_->Request(description, base::Closure());
}

void EncryptionMigrationScreenHandler::UpdateUIState(UIState state) {
  if (state == current_ui_state_)
    return;

  current_ui_state_ = state;
  CallJS("login.EncryptionMigrationScreen.setUIState", static_cast<int>(state));

  // When this handler is about to show the READY screen, we should get the
  // latest battery status and show it on the screen.
  if (state == UIState::READY)
    PowerManagerClient::Get()->RequestStatusUpdate();

  // We should request wake lock and not shut down on lid close during
  // migration.
  if (state == UIState::MIGRATING || state == UIState::MIGRATING_MINIMAL) {
    GetWakeLock()->RequestWakeLock();
    PowerPolicyController::Get()->SetEncryptionMigrationActive(true);
  } else {
    GetWakeLock()->CancelWakeLock();
    PowerPolicyController::Get()->SetEncryptionMigrationActive(false);
  }

  // Record which screen is visible to the user.
  // We record it after delay to make sure that the user was actually able
  // to see the screen (i.e. the screen is not just a flash).
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &EncryptionMigrationScreenHandler::OnDelayedRecordVisibleScreen,
          weak_ptr_factory_.GetWeakPtr(), state),
      base::TimeDelta::FromSeconds(1));
}

void EncryptionMigrationScreenHandler::CheckAvailableStorage() {
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      free_disk_space_fetcher_,
      base::Bind(&EncryptionMigrationScreenHandler::OnGetAvailableStorage,
                 weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnGetAvailableStorage(int64_t size) {
  if (size >= arc::kMigrationMinimumAvailableStorage || IsTestingUI()) {
    RecordFirstScreen(GetFirstScreenForMode(mode_));
    if (IsStartImmediately()) {
      if (IsMinimalMigration())
        StartMigration();
      else
        WaitBatteryAndMigrate();
    } else {
      UpdateUIState(UIState::READY);
    }
  } else {
    RecordFirstScreen(FirstScreen::FIRST_SCREEN_LOW_STORAGE);
    CallJS("login.EncryptionMigrationScreen.setAvailableSpaceInString",
           ui::FormatBytes(size));
    CallJS("login.EncryptionMigrationScreen.setNecessarySpaceInString",
           ui::FormatBytes(arc::kMigrationMinimumAvailableStorage));
    UpdateUIState(UIState::NOT_ENOUGH_STORAGE);
  }
}

void EncryptionMigrationScreenHandler::WaitBatteryAndMigrate() {
  if (current_battery_percent_) {
    if (*current_battery_percent_ >= arc::kMigrationMinimumBatteryPercent) {
      StartMigration();
      return;
    } else {
      // If migration was forced by policy, stop forcing it (we don't want the
      // user to have to wait until the battery is charged).
      MaybeStopForcingMigration();
    }
  }
  UpdateUIState(UIState::READY);

  should_migrate_on_enough_battery_ = true;
  PowerManagerClient::Get()->RequestStatusUpdate();
}

void EncryptionMigrationScreenHandler::StartMigration() {
  UpdateUIState(GetMigratingUIState());
  if (current_battery_percent_)
    initial_battery_percent_ = *current_battery_percent_;

  // Mount the existing eCryptfs vault to a temporary location for migration.
  cryptohome::MountRequest mount;
  cryptohome::AuthorizationRequest auth_request;
  mount.set_to_migrate_from_ecryptfs(true);
  if (IsArcKiosk()) {
    mount.set_public_mount(true);
  } else {
    auth_request = CreateAuthorizationRequest();
  }
  CryptohomeClient::Get()->MountEx(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context_.GetAccountId()),
      auth_request, mount,
      base::BindOnce(&EncryptionMigrationScreenHandler::OnMountExistingVault,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnMountExistingVault(
    base::Optional<cryptohome::BaseReply> reply) {
  cryptohome::MountError return_code =
      cryptohome::MountExReplyToMountError(reply);
  if (return_code != cryptohome::MOUNT_ERROR_NONE) {
    RecordMigrationResultMountFailure(IsResumingIncompleteMigration(),
                                      IsArcKiosk());
    UpdateUIState(UIState::MIGRATION_FAILED);
    LOG(ERROR) << "Mount existing vault failed. Error: " << return_code;
    return;
  }

  // For minimal migration, start a timer which will measure how long migration
  // took, so we can require a re sign-in if it took too long.
  if (IsMinimalMigration())
    minimal_migration_start_ = tick_clock_->NowTicks();

  cryptohome::MigrateToDircryptoRequest request;
  request.set_minimal_migration(IsMinimalMigration());
  CryptohomeClient::Get()->AddObserver(this);
  CryptohomeClient::Get()->MigrateToDircrypto(
      cryptohome::CreateAccountIdentifierFromAccountId(
          user_context_.GetAccountId()),
      request,
      base::Bind(&EncryptionMigrationScreenHandler::OnMigrationRequested,
                 weak_ptr_factory_.GetWeakPtr()));
}

device::mojom::WakeLock* EncryptionMigrationScreenHandler::GetWakeLock() {
  // |wake_lock_| is lazy bound and reused, even after a connection error.
  if (wake_lock_)
    return wake_lock_.get();

  mojo::PendingReceiver<device::mojom::WakeLock> receiver =
      wake_lock_.BindNewPipeAndPassReceiver();

  // Service manager connection might be not initialized in some testing
  // contexts.
  if (!content::GetSystemConnector())
    return wake_lock_.get();

  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  mojo::Remote<device::mojom::WakeLockProvider> wake_lock_provider;
  content::GetSystemConnector()->Connect(
      device::mojom::kServiceName,
      wake_lock_provider.BindNewPipeAndPassReceiver());
  wake_lock_provider->GetWakeLockWithoutContext(
      device::mojom::WakeLockType::kPreventAppSuspension,
      device::mojom::WakeLockReason::kOther,
      "Encryption migration is in progress...", std::move(receiver));
  return wake_lock_.get();
}

void EncryptionMigrationScreenHandler::RemoveCryptohome() {
  // Set invalid token status so that user is forced to go through Gaia on the
  // next sign-in.
  user_manager::UserManager::Get()->SaveUserOAuthStatus(
      user_context_.GetAccountId(),
      user_manager::User::OAUTH2_TOKEN_STATUS_INVALID);

  const cryptohome::Identification cryptohome_id(user_context_.GetAccountId());

  cryptohome::AccountIdentifier account_id_proto;
  account_id_proto.set_account_id(cryptohome_id.id());

  CryptohomeClient::Get()->RemoveEx(
      account_id_proto,
      base::BindOnce(&EncryptionMigrationScreenHandler::OnRemoveCryptohome,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EncryptionMigrationScreenHandler::OnRemoveCryptohome(
    base::Optional<cryptohome::BaseReply> reply) {
  cryptohome::MountError error = BaseReplyToMountError(reply);
  if (error == cryptohome::MOUNT_ERROR_NONE) {
    RecordRemoveCryptohomeResultSuccess(IsResumingIncompleteMigration(),
                                        IsArcKiosk());
  } else {
    LOG(ERROR) << "Removing cryptohome failed. return code: "
               << reply.value().error();
    RecordRemoveCryptohomeResultFailure(IsResumingIncompleteMigration(),
                                        IsArcKiosk());
  }

  UpdateUIState(UIState::MIGRATION_FAILED);
}

cryptohome::AuthorizationRequest
EncryptionMigrationScreenHandler::CreateAuthorizationRequest() {
  // |key| is created in the same manner as CryptohomeAuthenticator.
  const Key* key = user_context_.GetKey();
  // If the |key| is a plain text password, crash rather than attempting to
  // mount the cryptohome with a plain text password.
  CHECK_NE(Key::KEY_TYPE_PASSWORD_PLAIN, key->GetKeyType());
  cryptohome::AuthorizationRequest auth;
  cryptohome::Key* auth_key = auth.mutable_key();
  // Don't set the authorization's key label, implicitly setting it to an
  // empty string, which is a wildcard allowing any key to match. This is
  // necessary because cryptohomes created by Chrome OS M38 and older will have
  // a legacy key with no label while those created by Chrome OS M39 and newer
  // will have a key with the label kCryptohomeGAIAKeyLabel.
  auth_key->set_secret(key->GetSecret());
  return auth;
}

bool EncryptionMigrationScreenHandler::IsArcKiosk() const {
  return user_context_.GetUserType() == user_manager::USER_TYPE_ARC_KIOSK_APP;
}

void EncryptionMigrationScreenHandler::DircryptoMigrationProgress(
    cryptohome::DircryptoMigrationStatus status,
    uint64_t current,
    uint64_t total) {
  switch (status) {
    case cryptohome::DIRCRYPTO_MIGRATION_INITIALIZING:
      UpdateUIState(GetMigratingUIState());
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_IN_PROGRESS:
      UpdateUIState(GetMigratingUIState());
      CallJS("login.EncryptionMigrationScreen.setMigrationProgress",
             static_cast<double>(current) / total);
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_SUCCESS:
      RecordMigrationResultSuccess(IsResumingIncompleteMigration(),
                                   IsArcKiosk());
      // Stop listening to the progress updates.
      CryptohomeClient::Get()->RemoveObserver(this);
      // If the battery level decreased during migration, record the consumed
      // battery level.
      if (current_battery_percent_ &&
          *current_battery_percent_ < initial_battery_percent_) {
        UMA_HISTOGRAM_PERCENTAGE(
            kUmaNameConsumedBatteryPercent,
            static_cast<int>(std::round(initial_battery_percent_ -
                                        *current_battery_percent_)));
      }
      if (IsMinimalMigration() && !continue_login_callback_.is_null() &&
          !restart_login_callback_.is_null()) {
        GetWakeLock()->CancelWakeLock();
        PowerPolicyController::Get()->SetEncryptionMigrationActive(false);
        // If minimal migration was fast enough, continue with same sign-in
        // data. Fast enough means that the elapsed time is under the defined
        // threshold.
        const base::TimeDelta elapsed_time =
            tick_clock_->NowTicks() - minimal_migration_start_;
        const bool require_password_reentry =
            elapsed_time.InSeconds() >
            kMinimalMigrationReenterPasswordThreshold;
        if (require_password_reentry)
          std::move(restart_login_callback_).Run(user_context_);
        else
          std::move(continue_login_callback_).Run(user_context_);
      } else {
        // Restart immediately after successful full migration.
        PowerManagerClient::Get()->RequestRestart(
            power_manager::REQUEST_RESTART_OTHER,
            "login encryption migration success");
      }
      break;
    case cryptohome::DIRCRYPTO_MIGRATION_FAILED:
      RecordMigrationResultGeneralFailure(IsResumingIncompleteMigration(),
                                          IsArcKiosk());
      // Stop listening to the progress updates.
      CryptohomeClient::Get()->RemoveObserver(this);
      // Shows error screen after removing user directory is completed.
      RemoveCryptohome();
      break;
    default:
      break;
  }
}

void EncryptionMigrationScreenHandler::OnMigrationRequested(bool success) {
  if (!success) {
    LOG(ERROR) << "Requesting MigrateToDircrypto failed.";
    RecordMigrationResultRequestFailure(IsResumingIncompleteMigration(),
                                        IsArcKiosk());
    UpdateUIState(UIState::MIGRATION_FAILED);
  }
}

void EncryptionMigrationScreenHandler::OnDelayedRecordVisibleScreen(
    UIState ui_state) {
  if (current_ui_state_ != ui_state)
    return;

  // If |current_ui_state_| is not changed for a second, record the current
  // screen as a "visible" screen.
  UMA_HISTOGRAM_ENUMERATION(kUmaNameVisibleScreen, ui_state, UIState::COUNT);
}

bool EncryptionMigrationScreenHandler::IsResumingIncompleteMigration() const {
  return mode_ == EncryptionMigrationMode::RESUME_MIGRATION;
}

bool EncryptionMigrationScreenHandler::IsStartImmediately() const {
  return mode_ == EncryptionMigrationMode::START_MIGRATION ||
         mode_ == EncryptionMigrationMode::START_MINIMAL_MIGRATION ||
         mode_ == EncryptionMigrationMode::RESUME_MIGRATION ||
         mode_ == EncryptionMigrationMode::RESUME_MINIMAL_MIGRATION;
}

bool EncryptionMigrationScreenHandler::IsMinimalMigration() const {
  return mode_ == EncryptionMigrationMode::START_MINIMAL_MIGRATION ||
         mode_ == EncryptionMigrationMode::RESUME_MINIMAL_MIGRATION;
}

EncryptionMigrationScreenHandler::UIState
EncryptionMigrationScreenHandler::GetMigratingUIState() const {
  return IsMinimalMigration() ? UIState::MIGRATING_MINIMAL : UIState::MIGRATING;
}

void EncryptionMigrationScreenHandler::MaybeStopForcingMigration() {
  // |mode_| will be START_MIGRATION if migration was forced by user policy.
  // If an incomplete migration is being resumed, it would be RESUME_MIGRATION.
  // We only want to disable auto-starting migration in the first case.
  if (mode_ == EncryptionMigrationMode::START_MIGRATION ||
      mode_ == EncryptionMigrationMode::START_MINIMAL_MIGRATION)
    CallJS("login.EncryptionMigrationScreen.setIsResuming", false);
}

}  // namespace chromeos
