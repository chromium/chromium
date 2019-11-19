// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_mode.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/login/auth/user_context.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"
#include "third_party/cros_system_api/dbus/cryptohome/dbus-constants.h"

namespace base {
class TickClock;
class TimeTicks;
}  // namespace base

namespace chromeos {

class EncryptionMigrationScreen;
class LoginFeedback;
class UserContext;

class EncryptionMigrationScreenView {
 public:
  using ContinueLoginCallback = base::OnceCallback<void(const UserContext&)>;
  using RestartLoginCallback = base::OnceCallback<void(const UserContext&)>;

  constexpr static StaticOobeScreenId kScreenId{"encryption-migration"};

  virtual ~EncryptionMigrationScreenView() {}

  virtual void Show() = 0;
  virtual void Hide() = 0;
  virtual void SetDelegate(EncryptionMigrationScreen* delegate) = 0;
  virtual void SetUserContext(const UserContext& user_context) = 0;
  virtual void SetMode(EncryptionMigrationMode mode) = 0;
  virtual void SetContinueLoginCallback(ContinueLoginCallback callback) = 0;
  virtual void SetRestartLoginCallback(RestartLoginCallback callback) = 0;
  virtual void SetupInitialView() = 0;
};

// WebUI implementation of EncryptionMigrationScreenView
class EncryptionMigrationScreenHandler : public EncryptionMigrationScreenView,
                                         public BaseScreenHandler,
                                         public CryptohomeClient::Observer,
                                         public PowerManagerClient::Observer {
 public:
  using TView = EncryptionMigrationScreenView;

  explicit EncryptionMigrationScreenHandler(
      JSCallsContainer* js_calls_container);
  ~EncryptionMigrationScreenHandler() override;

  // EncryptionMigrationScreenView implementation:
  void Show() override;
  void Hide() override;
  void SetDelegate(EncryptionMigrationScreen* delegate) override;
  void SetUserContext(const UserContext& user_context) override;
  void SetMode(EncryptionMigrationMode mode) override;
  void SetContinueLoginCallback(ContinueLoginCallback callback) override;
  void SetRestartLoginCallback(RestartLoginCallback callback) override;
  void SetupInitialView() override;

  // BaseScreenHandler implementation:
  void DeclareLocalizedValues(
      ::login::LocalizedValuesBuilder* builder) override;
  void Initialize() override;

  // Callback that can be used to check free disk space.
  using FreeDiskSpaceFetcher = base::RepeatingCallback<int64_t()>;

  // Testing only: Sets the free disk space fetcher.
  void SetFreeDiskSpaceFetcherForTesting(
      FreeDiskSpaceFetcher free_disk_space_fetcher);

  // Testing only: Sets the tick clock used to measure elapsed time during
  // migration.
  // This doesn't take the ownership of the clock. |tick_clock| must outlive the
  // EncryptionMigrationScreenHandler instance.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

 protected:
  virtual device::mojom::WakeLock* GetWakeLock();

 private:
  // Enumeration for migration UI state. These values must be kept in sync with
  // EncryptionMigrationUIState in JS code, and match the numbering for
  // MigrationUIScreen in histograms/enums.xml. Do not reorder or remove items,
  // only add new items before COUNT.
  enum UIState {
    INITIAL = 0,
    READY = 1,
    MIGRATING = 2,
    MIGRATION_FAILED = 3,
    NOT_ENOUGH_STORAGE = 4,
    MIGRATING_MINIMAL = 5,
    COUNT
  };

  // WebUIMessageHandler implementation:
  void RegisterMessages() override;

  // PowerManagerClient::Observer implementation:
  void PowerChanged(const power_manager::PowerSupplyProperties& proto) override;

  // Handlers for JS API callbacks.
  void HandleStartMigration();
  void HandleSkipMigration();
  void HandleRequestRestartOnLowStorage();
  void HandleRequestRestartOnFailure();
  void HandleOpenFeedbackDialog();

  // Updates UI state.
  void UpdateUIState(UIState state);

  void CheckAvailableStorage();
  void OnGetAvailableStorage(int64_t size);
  void WaitBatteryAndMigrate();
  void StartMigration();
  void OnMountExistingVault(base::Optional<cryptohome::BaseReply> reply);
  // Removes cryptohome and shows the error screen after the removal finishes.
  void RemoveCryptohome();
  void OnRemoveCryptohome(base::Optional<cryptohome::BaseReply> reply);

  // Creates authorization request for MountEx method using |user_context_|.
  cryptohome::AuthorizationRequest CreateAuthorizationRequest();

  // True if the session is in ARC kiosk mode.
  bool IsArcKiosk() const;

  // CryptohomeClient::Observer implementation:
  void DircryptoMigrationProgress(cryptohome::DircryptoMigrationStatus status,
                                  uint64_t current,
                                  uint64_t total) override;

  // Handlers for cryptohome API callbacks.
  void OnMigrationRequested(bool success);

  // Records UMA about visible screen after delay.
  void OnDelayedRecordVisibleScreen(UIState state);

  // True if |mode_| suggests that we are resuming an incomplete migration.
  bool IsResumingIncompleteMigration() const;

  // True if |mode_| suggests that migration should start immediately.
  bool IsStartImmediately() const;

  // True if |mode_| suggests that we are starting or resuming a minimal
  // migration.
  bool IsMinimalMigration() const;

  // Returns the UIState we should be in when migration is in progress.
  // This will be different between regular and minimal migration.
  UIState GetMigratingUIState() const;

  // Stop forcing migration if it was forced by policy.
  void MaybeStopForcingMigration();

  EncryptionMigrationScreen* delegate_ = nullptr;
  bool show_on_init_ = false;

  // The current UI state which should be refrected in the web UI.
  UIState current_ui_state_ = INITIAL;

  // The current user's UserContext, which is used to request the migration to
  // cryptohome.
  UserContext user_context_;

  // The callback which is used to log in to the session from the migration UI.
  ContinueLoginCallback continue_login_callback_;

  // The callback which is used to require the user to re-enter their password.
  RestartLoginCallback restart_login_callback_;

  // The migration mode (ask user / start migration automatically / resume
  // incomplete migratoin).
  EncryptionMigrationMode mode_ = EncryptionMigrationMode::ASK_USER;

  // The current battery level.
  base::Optional<double> current_battery_percent_;

  // True if the migration should start immediately once the battery level gets
  // sufficient.
  bool should_migrate_on_enough_battery_ = false;

  // The battery level at the timing that the migration starts.
  double initial_battery_percent_ = 0.0;

  // Point in time when minimal migration started, as reported by |tick_clock_|.
  base::TimeTicks minimal_migration_start_;

  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  std::unique_ptr<LoginFeedback> login_feedback_;

  // Used to measure elapsed time during migration.
  const base::TickClock* tick_clock_;

  FreeDiskSpaceFetcher free_disk_space_fetcher_;

  base::WeakPtrFactory<EncryptionMigrationScreenHandler> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(EncryptionMigrationScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENCRYPTION_MIGRATION_SCREEN_HANDLER_H_
