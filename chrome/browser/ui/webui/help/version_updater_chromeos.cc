// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/version_updater_chromeos.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/policy/core/common/management/management_service.h"
#include "content/public/browser/web_contents.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using ::ash::OwnerSettingsServiceAsh;
using ::ash::OwnerSettingsServiceAshFactory;
using ::ash::UpdateEngineClient;

// Network status in the context of device update.
enum NetworkStatus {
  // It's allowed to use current network for update.
  NETWORK_STATUS_ALLOWED = 0,
  // It's disallowed to use current network for update.
  NETWORK_STATUS_DISALLOWED,
  // Device is in offline state.
  NETWORK_STATUS_OFFLINE
};

const bool kDefaultAutoUpdateDisabled = false;

NetworkStatus GetNetworkStatus(bool interactive,
                               const ash::NetworkState* network,
                               bool metered) {
  if (!network || !network->IsConnectedState())  // Offline state.
    return NETWORK_STATUS_OFFLINE;

  if (metered &&
      !help_utils_chromeos::IsUpdateOverCellularAllowed(interactive)) {
    return NETWORK_STATUS_DISALLOWED;
  }
  return NETWORK_STATUS_ALLOWED;
}

// Returns true if auto-update is disabled by the system administrator.
bool IsAutoUpdateDisabled() {
  bool update_disabled = kDefaultAutoUpdateDisabled;
  ash::CrosSettings* settings = ash::CrosSettings::Get();
  if (!settings)
    return update_disabled;
  const base::Value* update_disabled_value =
      settings->GetPref(ash::kUpdateDisabled);
  if (update_disabled_value) {
    CHECK(update_disabled_value->is_bool());
    update_disabled = update_disabled_value->GetBool();
  }
  return update_disabled;
}

std::u16string GetConnectionTypeAsUTF16(const ash::NetworkState* network,
                                        bool metered) {
  const std::string type = network->type();
  if (ash::NetworkTypePattern::WiFi().MatchesType(type)) {
    if (metered)
      return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_METERED_WIFI);
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_WIFI);
  }
  if (ash::NetworkTypePattern::Ethernet().MatchesType(type))
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_ETHERNET);
  if (ash::NetworkTypePattern::Mobile().MatchesType(type))
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_MOBILE_DATA);
  if (ash::NetworkTypePattern::VPN().MatchesType(type))
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_VPN);
  NOTREACHED_IN_MIGRATION();
  return std::u16string();
}

// Returns whether an update is allowed. If not, it calls the callback with
// the appropriate status. |interactive| indicates whether the user is actively
// checking for updates.
bool EnsureCanUpdate(bool interactive,
                     const VersionUpdater::StatusCallback& callback) {
  if (IsAutoUpdateDisabled()) {
    callback.Run(VersionUpdater::DISABLED_BY_ADMIN, 0, false, false,
                 std::string(), 0,
                 l10n_util::GetStringUTF16(IDS_UPGRADE_DISABLED_BY_POLICY));
    return false;
  }

  ash::NetworkStateHandler* network_state_handler =
      ash::NetworkHandler::Get()->network_state_handler();
  const ash::NetworkState* network = network_state_handler->DefaultNetwork();
  const bool metered = network_state_handler->default_network_is_metered();
  // Don't allow an update if we're currently offline or connected
  // to a network for which updates are disallowed.
  NetworkStatus status = GetNetworkStatus(interactive, network, metered);
  if (status == NETWORK_STATUS_OFFLINE) {
    callback.Run(VersionUpdater::FAILED_OFFLINE, 0, false, false, std::string(),
                 0, l10n_util::GetStringUTF16(IDS_UPGRADE_OFFLINE));
    return false;
  } else if (status == NETWORK_STATUS_DISALLOWED) {
    std::u16string message = l10n_util::GetStringFUTF16(
        IDS_UPGRADE_DISALLOWED, GetConnectionTypeAsUTF16(network, metered));
    callback.Run(VersionUpdater::FAILED_CONNECTION_TYPE_DISALLOWED, 0, false,
                 false, std::string(), 0, message);
    return false;
  }

  return true;
}

}  // namespace

std::unique_ptr<VersionUpdater> VersionUpdater::Create(
    content::WebContents* web_contents) {
  return base::WrapUnique(new VersionUpdaterCros(web_contents));
}

void VersionUpdaterCros::GetUpdateStatus(StatusCallback callback) {
  callback_ = std::move(callback);

  // User is not actively checking for updates.
  if (!EnsureCanUpdate(false /* interactive */, callback_))
    return;

  UpdateEngineClient* update_engine_client = UpdateEngineClient::Get();
  if (!update_engine_client->HasObserver(this))
    update_engine_client->AddObserver(this);

  this->UpdateStatusChanged(update_engine_client->GetLastStatus());
}

void VersionUpdaterCros::ApplyDeferredUpdate() {
  UpdateEngineClient* update_engine_client = UpdateEngineClient::Get();

  DCHECK(update_engine_client->GetLastStatus().current_operation() ==
         update_engine::Operation::UPDATED_BUT_DEFERRED);

  update_engine_client->ApplyDeferredUpdate(/*shutdown_after_update=*/false,
                                            base::DoNothing());
}

void VersionUpdaterCros::CheckForUpdate(StatusCallback callback,
                                        PromoteCallback) {
  callback_ = std::move(callback);

  // User is actively checking for updates.
  if (!EnsureCanUpdate(true /* interactive */, callback_))
    return;

  UpdateEngineClient* update_engine_client = UpdateEngineClient::Get();
  if (!update_engine_client->HasObserver(this))
    update_engine_client->AddObserver(this);

  if (update_engine_client->GetLastStatus().current_operation() !=
      update_engine::Operation::IDLE) {
    check_for_update_when_idle_ = true;
    return;
  }
  check_for_update_when_idle_ = false;

  // Make sure that libcros is loaded and OOBE is complete.
  if (!ash::WizardController::default_controller() ||
      ash::StartupUtils::IsDeviceRegistered()) {
    update_engine_client->RequestUpdateCheck(base::BindOnce(
        &VersionUpdaterCros::OnUpdateCheck, weak_ptr_factory_.GetWeakPtr()));
  }
}

void VersionUpdaterCros::SetChannel(const std::string& channel,
                                    bool is_powerwash_allowed) {
  OwnerSettingsServiceAsh* service =
      context_
          ? OwnerSettingsServiceAshFactory::GetInstance()->GetForBrowserContext(
                context_)
          : nullptr;
  // For local owner set the field in the policy blob.
  if (service)
    service->SetString(ash::kReleaseChannel, channel);
  UpdateEngineClient::Get()->SetChannel(channel, is_powerwash_allowed);
}

void VersionUpdaterCros::SetUpdateOverCellularOneTimePermission(
    StatusCallback callback,
    const std::string& update_version,
    int64_t update_size) {
  callback_ = std::move(callback);
  UpdateEngineClient::Get()->SetUpdateOverCellularOneTimePermission(
      update_version, update_size,
      base::BindOnce(
          &VersionUpdaterCros::OnSetUpdateOverCellularOneTimePermission,
          weak_ptr_factory_.GetWeakPtr()));
}

void VersionUpdaterCros::OnSetUpdateOverCellularOneTimePermission(
    bool success) {
  if (success) {
    // One time permission is set successfully, so we can proceed to update.
    CheckForUpdate(callback_, VersionUpdater::PromoteCallback());
  } else {
    // TODO(crbug.com/40612027): invoke callback to signal about page to
    // show appropriate error message.
    LOG(ERROR) << "Error setting update over cellular one time permission.";
    callback_.Run(VersionUpdater::FAILED, 0, false, false, std::string(), 0,
                  std::u16string());
  }
}

void VersionUpdaterCros::GetChannel(bool get_current_channel,
                                    ChannelCallback cb) {
  // Request the channel information. Bind to a weak_ptr bound method rather
  // than passing |cb| directly so that |cb| does not outlive |this|.
  UpdateEngineClient::Get()->GetChannel(
      get_current_channel,
      base::BindOnce(&VersionUpdaterCros::OnGetChannel,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cb)));
}

void VersionUpdaterCros::OnGetChannel(ChannelCallback cb,
                                      const std::string& current_channel) {
  std::move(cb).Run(current_channel);
}

void VersionUpdaterCros::GetEolInfo(EolInfoCallback cb) {
  // Request the EolInfo. Bind to a weak_ptr bound method rather than passing
  // |cb| directly so that |cb| does not outlive |this|.
  UpdateEngineClient::Get()->GetEolInfo(
      base::BindOnce(&VersionUpdaterCros::OnGetEolInfo,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cb)));
}

void VersionUpdaterCros::OnGetEolInfo(EolInfoCallback cb,
                                      UpdateEngineClient::EolInfo eol_info) {
  std::move(cb).Run(std::move(eol_info));
}

void VersionUpdaterCros::ToggleFeature(const std::string& feature,
                                       bool enable) {
  UpdateEngineClient::Get()->ToggleFeature(feature, enable);
}

void VersionUpdaterCros::IsFeatureEnabled(const std::string& feature,
                                          IsFeatureEnabledCallback callback) {
  UpdateEngineClient::Get()->IsFeatureEnabled(
      feature,
      base::BindOnce(&VersionUpdaterCros::OnIsFeatureEnabled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void VersionUpdaterCros::OnIsFeatureEnabled(IsFeatureEnabledCallback callback,
                                            std::optional<bool> enabled) {
  std::move(callback).Run(std::move(enabled));
}

bool VersionUpdaterCros::IsManagedAutoUpdateEnabled() {
  return !IsAutoUpdateDisabled();
}

VersionUpdaterCros::VersionUpdaterCros(content::WebContents* web_contents)
    : context_(web_contents ? web_contents->GetBrowserContext() : nullptr),
      last_operation_(update_engine::Operation::IDLE),
      check_for_update_when_idle_(false) {}

VersionUpdaterCros::~VersionUpdaterCros() {
  UpdateEngineClient::Get()->RemoveObserver(this);
}

void VersionUpdaterCros::UpdateStatusChanged(
    const update_engine::StatusResult& status) {
  Status my_status = UPDATED;
  int progress = 0;
  std::string version = status.new_version();
  int64_t size = status.new_size();
  std::u16string message;

  // If the status change is for an installation, this means that DLCs are being
  // installed and has nothing to with the OS. Ignore this status change.
  if (status.is_install())
    return;

  // If the updater is currently idle, just show the last operation (unless it
  // was previously checking for an update -- in that case, the system is
  // up to date now).  See http://crbug.com/120063 for details.
  update_engine::Operation operation_to_show = status.current_operation();
  if (status.current_operation() == update_engine::Operation::IDLE &&
      last_operation_ != update_engine::Operation::CHECKING_FOR_UPDATE) {
    operation_to_show = last_operation_;
  }

  switch (operation_to_show) {
    case update_engine::Operation::IDLE:
    case update_engine::Operation::DISABLED:
    case update_engine::Operation::ERROR:
    case update_engine::Operation::REPORTING_ERROR_EVENT:
    case update_engine::Operation::ATTEMPTING_ROLLBACK:
    case update_engine::Operation::CLEANUP_PREVIOUS_UPDATE:
      // Update engine reports errors for some conditions that shouldn't
      // actually be displayed as errors to users so leave the status as
      // UPDATED. However for some specific errors use the specific FAILED
      // statuses. Last attempt error remains when update engine state is
      // idle.

      if (status.last_attempt_error() ==
          static_cast<int32_t>(
              update_engine::ErrorCode::kOmahaUpdateIgnoredPerPolicy)) {
        if (policy::ManagementServiceFactory::GetForPlatform()->IsManaged()) {
          my_status = DISABLED_BY_ADMIN;
        } else {
          // Handle the special case where after a consumer rollback,
          // updating to the previously installed version just rolledback from
          // is disallowed.
          // TODO(b/277962165) Update the platform side to expose a more
          // specific error code for this case.
          my_status = UPDATE_TO_ROLLBACK_VERSION_DISALLOWED;
        }
      } else if (status.last_attempt_error() ==
                 static_cast<int32_t>(
                     update_engine::ErrorCode::kOmahaErrorInHTTPResponse)) {
        my_status = FAILED_HTTP;
      } else if (status.last_attempt_error() ==
                 static_cast<int32_t>(
                     update_engine::ErrorCode::kDownloadTransferError)) {
        my_status = FAILED_DOWNLOAD;
      }
      break;
    case update_engine::Operation::CHECKING_FOR_UPDATE:
      my_status = CHECKING;
      break;
    case update_engine::Operation::DOWNLOADING:
      progress = static_cast<int>(round(status.progress() * 100));
      [[fallthrough]];
    case update_engine::Operation::UPDATE_AVAILABLE:
      my_status = UPDATING;
      break;
    case update_engine::Operation::NEED_PERMISSION_TO_UPDATE:
      my_status = NEED_PERMISSION_TO_UPDATE;
      break;
    case update_engine::Operation::VERIFYING:
    case update_engine::Operation::FINALIZING:
      // Once the download is finished, keep the progress at 100; it shouldn't
      // go down while the status is the same.
      progress = 100;
      my_status = UPDATING;
      break;
    case update_engine::Operation::UPDATED_NEED_REBOOT:
      my_status = NEARLY_UPDATED;
      break;
    case update_engine::Operation::UPDATED_BUT_DEFERRED:
      my_status = DEFERRED;
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  // If the current auto update is non-interactive and will be deferred, ignore
  // update status change and show UPDATED instead. The NEARLY_UPDATED or
  // DEFERRED status will still be shown, because user may need to interact with
  // UI to apply the update and reboot the device.
  if (my_status != NEARLY_UPDATED && my_status != DEFERRED &&
      !status.is_interactive() && status.will_defer_update()) {
    my_status = UPDATED;
    progress = 0;
  }

  callback_.Run(my_status, progress, status.is_enterprise_rollback(),
                status.will_powerwash_after_reboot(), version, size, message);
  last_operation_ = status.current_operation();

  if (check_for_update_when_idle_ &&
      status.current_operation() == update_engine::Operation::IDLE) {
    CheckForUpdate(callback_, VersionUpdater::PromoteCallback());
  }
}

void VersionUpdaterCros::OnUpdateCheck(
    UpdateEngineClient::UpdateCheckResult result) {
  // If version updating is not implemented, this binary is the most up-to-date
  // possible with respect to automatic updating.
  if (result == UpdateEngineClient::UPDATE_RESULT_NOTIMPLEMENTED)
    callback_.Run(UPDATED, 0, false, false, std::string(), 0, std::u16string());
}
