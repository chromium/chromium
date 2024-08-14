// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_host.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/drivefs_http_client.h"
#include "chromeos/ash/components/drivefs/drivefs_search.h"
#include "chromeos/ash/components/drivefs/drivefs_search_query.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/mojom/notifications.mojom.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace drivefs {

namespace {

constexpr char kDataPath[] = "GCache/v2";

}  // namespace

std::ostream& operator<<(std::ostream& os, const drivefs::SyncStatus& status) {
  switch (status) {
    case SyncStatus::kNotFound:
      return os << "not_found";
    case SyncStatus::kCompleted:
      return os << "completed";
    case SyncStatus::kQueued:
      return os << "queued";
    case SyncStatus::kInProgress:
      return os << "in_progress";
    case SyncStatus::kError:
      return os << "error";
    default:
      return os << "unknown";
  }
}

std::unique_ptr<DriveFsBootstrapListener>
DriveFsHost::Delegate::CreateMojoListener() {
  return std::make_unique<DriveFsBootstrapListener>();
}

// A container of state tied to a particular mounting of DriveFS. None of this
// should be shared between mounts.
class DriveFsHost::MountState : public DriveFsSession {
 public:
  explicit MountState(DriveFsHost* host)
      : DriveFsSession(host->timer_.get(),
                       DiskMounter::Create(host->disk_mount_manager_),
                       CreateMojoConnection(host->account_token_delegate_.get(),
                                            host->delegate_),
                       host->GetDataPath(),
                       host->delegate_->GetMyFilesPath(),
                       host->GetDefaultMountDirName(),
                       host->mount_observer_),
        host_(host) {
    token_fetch_attempted_ =
        bool{host->account_token_delegate_->GetCachedAccessToken()};
    search_ = std::make_unique<DriveFsSearch>(
        drivefs_interface(), host_->network_connection_tracker_, host_->clock_);
    if (base::FeatureList::IsEnabled(ash::features::kDriveFsChromeNetworking)) {
      http_client_ = std::make_unique<DriveFsHttpClient>(
          host_->delegate_->GetURLLoaderFactory());
    }
  }

  MountState(const MountState&) = delete;
  MountState& operator=(const MountState&) = delete;

  ~MountState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (is_mounted()) {
      for (Observer& observer : host_->observers_) {
        DCHECK_EQ(observer.GetHost(), host_);
        observer.OnUnmounted();
      }
    }
  }

  static std::unique_ptr<DriveFsConnection> CreateMojoConnection(
      DriveFsAuth* auth_delegate,
      DriveFsHost::Delegate* delegate) {
    auto access_token = auth_delegate->GetCachedAccessToken();
    mojom::DriveFsConfigurationPtr config = {
        std::in_place,
        auth_delegate->GetAccountId().GetUserEmail(),
        std::move(access_token),
        auth_delegate->IsMetricsCollectionEnabled(),
        delegate->GetLostAndFoundDirectoryName(),
        base::FeatureList::IsEnabled(ash::features::kDriveFsMirroring),
        delegate->IsVerboseLoggingEnabled(),
        base::FeatureList::IsEnabled(ash::features::kDriveFsChromeNetworking),
        base::FeatureList::IsEnabled(ash::features::kDriveFsShowCSEFiles)
            ? mojom::CSESupport::kListing
            : mojom::CSESupport::kNone,
        ash::features::IsLauncherContinueSectionWithRecentsEnabled(),
        ash::features::IsShowSharingUserInLauncherContinueSectionEnabled(),
    };
    return DriveFsConnection::Create(delegate->CreateMojoListener(),
                                     std::move(config));
  }

  std::unique_ptr<DriveFsSearchQuery> CreateSearchQuery(
      mojom::QueryParametersPtr query) {
    return search_->CreateQuery(std::move(query));
  }

  mojom::QueryParameters::QuerySource SearchDriveFs(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback) {
    return search_->PerformSearch(std::move(query), std::move(callback));
  }

 private:
  // mojom::DriveFsDelegate:
  void GetAccessToken(const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      GetAccessTokenCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->account_token_delegate_->GetAccessToken(
        !token_fetch_attempted_,
        base::BindOnce(
            [](GetAccessTokenCallback callback, mojom::AccessTokenStatus status,
               mojom::AccessTokenPtr access_token) {
              if (status != mojom::AccessTokenStatus::kSuccess) {
                std::move(callback).Run(status, "");
                return;
              }
              std::move(callback).Run(status, access_token->token);
            },
            std::move(callback)));
    token_fetch_attempted_ = true;
  }

  void GetAccessTokenWithExpiry(
      const std::string& client_id,
      const std::string& app_id,
      const std::vector<std::string>& scopes,
      GetAccessTokenWithExpiryCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->account_token_delegate_->GetAccessToken(!token_fetch_attempted_,
                                                   std::move(callback));
    token_fetch_attempted_ = true;
  }

  void OnItemProgress(const mojom::ProgressEventPtr progress_event) override {
    for (Observer& observer : host_->observers_) {
      DCHECK_EQ(observer.GetHost(), host_);
      observer.OnItemProgress(*progress_event);
    }
  }

  void OnSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);

    for (auto& observer : host_->observers_) {
      observer.OnSyncingStatusUpdate(*status);
    }
  }

  void OnMirrorSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {
    for (Observer& observer : host_->observers_) {
      DCHECK_EQ(observer.GetHost(), host_);
      observer.OnMirrorSyncingStatusUpdate(*status);
    }
  }

  void OnFilesChanged(std::vector<mojom::FileChangePtr> changes) override {
    std::vector<mojom::FileChange> changes_values;
    changes_values.reserve(changes.size());
    for (mojom::FileChangePtr& change : changes) {
      changes_values.emplace_back(std::move(*change));
    }
    for (Observer& observer : host_->observers_) {
      DCHECK_EQ(observer.GetHost(), host_);
      observer.OnFilesChanged(changes_values);
    }
  }

  void OnError(mojom::DriveErrorPtr error) override {
    if (!IsKnownEnumValue(error->type)) {
      return;
    }
    for (Observer& observer : host_->observers_) {
      DCHECK_EQ(observer.GetHost(), host_);
      observer.OnError(*error);
    }
  }

  void OnTeamDrivesListReady(
      const std::vector<std::string>& team_drive_ids) override {}

  void OnTeamDriveChanged(const std::string& team_drive_id,
                          CreateOrDelete change_type) override {}

  void ConnectToExtension(
      mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<mojom::NativeMessagingPort> port,
      mojo::PendingRemote<mojom::NativeMessagingHost> host,
      ConnectToExtensionCallback callback) override {
    host_->delegate_->ConnectToExtension(std::move(params), std::move(port),
                                         std::move(host), std::move(callback));
  }

  void DisplayConfirmDialog(mojom::DialogReasonPtr error,
                            DisplayConfirmDialogCallback callback) override {
    if (!IsKnownEnumValue(error->type) || !host_->dialog_handler_) {
      std::move(callback).Run(mojom::DialogResult::kNotDisplayed);
      return;
    }
    host_->dialog_handler_.Run(
        *error, mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                    std::move(callback), mojom::DialogResult::kNotDisplayed));
  }

  void ExecuteHttpRequest(
      mojom::HttpRequestPtr request,
      mojo::PendingRemote<mojom::HttpDelegate> delegate) override {
    if (!http_client_) {
      // The Chrome Network Service <-> DriveFS bridge is not enabled. Ignore
      // the request and allow the |delegate| to close itself. DriveFS will
      // pick up on the |delegate| closure and fallback to cURL.
      return;
    }
    http_client_->ExecuteHttpRequest(std::move(request), std::move(delegate));
  }

  void GetMachineRootID(GetMachineRootIDCallback callback) override {
    if (!ash::features::IsDriveFsMirroringEnabled()) {
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(host_->delegate_->GetMachineRootID());
  }

  void PersistMachineRootID(const std::string& id) override {
    if (!ash::features::IsDriveFsMirroringEnabled()) {
      return;
    }
    host_->delegate_->PersistMachineRootID(std::move(id));
  }

  void OnNotificationReceived(
      mojom::DriveFsNotificationPtr notification) override {
    if (!ash::features::IsDriveFsMirroringEnabled()) {
      return;
    }
    host_->delegate_->PersistNotification(std::move(notification));
  }

  void OnMirrorSyncError(mojom::MirrorSyncErrorListPtr error_list) override {
    if (ash::features::IsDriveFsMirroringEnabled()) {
      host_->delegate_->PersistSyncErrors(std::move(error_list));
    }
  }

  // Owns |this|.
  const raw_ptr<DriveFsHost> host_;

  std::unique_ptr<DriveFsSearch> search_;
  std::unique_ptr<DriveFsHttpClient> http_client_;

  bool token_fetch_attempted_ = false;

  // Used to dispatch individual sync status updates in a debounced manner, only
  // sending the sync states that have changed since the last dispatched event.
  std::unique_ptr<base::RetainingOneShotTimer> sync_throttle_timer_;
};

DriveFsHost::DriveFsHost(
    const base::FilePath& profile_path,
    DriveFsHost::Delegate* delegate,
    DriveFsHost::MountObserver* mount_observer,
    network::NetworkConnectionTracker* network_connection_tracker,
    const base::Clock* clock,
    ash::disks::DiskMountManager* disk_mount_manager,
    std::unique_ptr<base::OneShotTimer> timer)
    : profile_path_(profile_path),
      delegate_(delegate),
      mount_observer_(mount_observer),
      network_connection_tracker_(network_connection_tracker),
      clock_(clock),
      disk_mount_manager_(disk_mount_manager),
      timer_(std::move(timer)),
      account_token_delegate_(
          std::make_unique<DriveFsAuth>(clock,
                                        profile_path,
                                        std::make_unique<base::OneShotTimer>(),
                                        delegate)) {
  DCHECK(delegate_);
  DCHECK(mount_observer_);
  DCHECK(network_connection_tracker_);
  DCHECK(clock_);
}

DriveFsHost::~DriveFsHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (Observer& observer : observers_) {
    DCHECK_EQ(observer.GetHost(), this);
    observer.OnHostDestroyed();
    observer.Reset();
  }

  // Reset `mount_state_` manually to avoid accessing a partially-destructed
  // `this` in ~MountState().
  mount_state_.reset();
}

bool DriveFsHost::Mount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(b/336831215): Remove these logs once bug has been fixed.
  LOG(ERROR) << "DriveFs mounted";
  const AccountId& account_id = delegate_->GetAccountId();
  if (mount_state_ || !account_id.HasAccountIdKey() ||
      account_id.GetUserEmail().empty()) {
    return false;
  }
  mount_state_ = std::make_unique<MountState>(this);
  return true;
}

void DriveFsHost::Unmount() {
  // TODO(b/336831215): Remove these logs once bug has been fixed.
  LOG(ERROR) << "DriveFs unmounted";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mount_state_.reset();
}

bool DriveFsHost::IsMounted() const {
  return mount_state_ && mount_state_->is_mounted();
}

base::FilePath DriveFsHost::GetMountPath() const {
  return mount_state_ && mount_state_->is_mounted()
             ? mount_state_->mount_path()
             : base::FilePath("/media/fuse").Append(GetDefaultMountDirName());
}

base::FilePath DriveFsHost::GetDataPath() const {
  return profile_path_.Append(kDataPath).Append(
      delegate_->GetObfuscatedAccountId());
}

mojom::DriveFs* DriveFsHost::GetDriveFsInterface() const {
  if (!mount_state_ || !mount_state_->is_mounted()) {
    return nullptr;
  }
  return mount_state_->drivefs_interface();
}

std::unique_ptr<DriveFsSearchQuery> DriveFsHost::CreateSearchQuery(
    mojom::QueryParametersPtr query) {
  if (!mount_state_ || !mount_state_->is_mounted()) {
    return nullptr;
  }
  return mount_state_->CreateSearchQuery(std::move(query));
}

mojom::QueryParameters::QuerySource DriveFsHost::PerformSearch(
    mojom::QueryParametersPtr query,
    mojom::SearchQuery::GetNextPageCallback callback) {
  if (!mount_state_ || !mount_state_->is_mounted()) {
    std::move(callback).Run(drive::FileError::FILE_ERROR_SERVICE_UNAVAILABLE,
                            {});
    return mojom::QueryParameters::QuerySource::kLocalOnly;
  }
  return mount_state_->SearchDriveFs(std::move(query), std::move(callback));
}

std::string DriveFsHost::GetDefaultMountDirName() const {
  return base::StrCat({"drivefs-", delegate_->GetObfuscatedAccountId()});
}

DriveFsHost::Observer::~Observer() {
  Reset();
}

void DriveFsHost::Observer::Observe(DriveFsHost* const host) {
  if (host != host_) {
    Reset();

    if (host) {
      host->observers_.AddObserver(this);
      host_ = host;
    }
  }
}

void DriveFsHost::Observer::Reset() {
  if (host_) {
    host_->observers_.RemoveObserver(this);
    host_ = nullptr;
  }

  DCHECK(!IsInObserverList());
}

}  // namespace drivefs
