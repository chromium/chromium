// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_host.h"

#include <map>
#include <set>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/ash/components/drivefs/drivefs_host_observer.h"
#include "chromeos/ash/components/drivefs/drivefs_http_client.h"
#include "chromeos/ash/components/drivefs/drivefs_search.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace drivefs {

namespace {

constexpr char kDataPath[] = "GCache/v2";

}  // namespace

std::unique_ptr<DriveFsBootstrapListener>
DriveFsHost::Delegate::CreateMojoListener() {
  return std::make_unique<DriveFsBootstrapListener>();
}

// A container of state tied to a particular mounting of DriveFS. None of this
// should be shared between mounts.
class DriveFsHost::MountState : public DriveFsSession,
                                public drive::DriveNotificationObserver {
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
        host_(host),
        sync_status_tracker_(std::make_unique<SyncStatusTracker>()) {
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
    if (team_drives_fetched_) {
      host_->delegate_->GetDriveNotificationManager().ClearTeamDriveIds();
      host_->delegate_->GetDriveNotificationManager().RemoveObserver(this);
    }
    if (is_mounted()) {
      for (auto& observer : host_->observers_) {
        observer.OnUnmounted();
      }
    }
  }

  static std::unique_ptr<DriveFsConnection> CreateMojoConnection(
      DriveFsAuth* auth_delegate,
      DriveFsHost::Delegate* delegate) {
    auto access_token = auth_delegate->GetCachedAccessToken();
    mojom::DriveFsConfigurationPtr config = {
        absl::in_place,
        auth_delegate->GetAccountId().GetUserEmail(),
        std::move(access_token),
        auth_delegate->IsMetricsCollectionEnabled(),
        delegate->GetLostAndFoundDirectoryName(),
        base::FeatureList::IsEnabled(ash::features::kDriveFsMirroring),
        delegate->IsVerboseLoggingEnabled(),
        base::FeatureList::IsEnabled(ash::features::kDriveFsChromeNetworking),
    };
    return DriveFsConnection::Create(delegate->CreateMojoListener(),
                                     std::move(config));
  }

  mojom::QueryParameters::QuerySource SearchDriveFs(
      mojom::QueryParametersPtr query,
      mojom::SearchQuery::GetNextPageCallback callback) {
    return search_->PerformSearch(std::move(query), std::move(callback));
  }

  SyncStatusAndProgress GetSyncStatusForPath(const base::FilePath& drive_path) {
    return sync_status_tracker_->GetSyncStatusForPath(drive_path);
  }

 private:
  // mojom::DriveFsDelegate:
  void GetAccessToken(const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      GetAccessTokenCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->account_token_delegate_->GetAccessToken(!token_fetch_attempted_,
                                                   std::move(callback));
    token_fetch_attempted_ = true;
  }

  void OnSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {
    if (base::FeatureList::IsEnabled(ash::features::kFilesInlineSyncStatus)) {
      // Keep track of the syncing paths.
      bool has_invalid_progress = false;
      for (const mojom::ItemEventPtr& event : status->item_events) {
        base::FilePath path = host_->GetMountPath();
        if (!base::FilePath("/").AppendRelativePath(base::FilePath(event->path),
                                                    &path)) {
          LOG(ERROR) << "Failed to make path relative to drive root";
          continue;
        }
        switch (event->state) {
          case mojom::ItemEvent::State::kQueued:
            sync_status_tracker_->AddSyncStatusForPath(event->stable_id, path,
                                                       SyncStatus::kQueued, 0);
            break;
          case mojom::ItemEvent::State::kInProgress: {
            float transferred = event->bytes_transferred;
            float total = event->bytes_to_transfer;
            float progress = -1;
            if (total > 0 && transferred <= total) {
              progress = transferred / total;
            } else {
              has_invalid_progress = true;
            }
            sync_status_tracker_->AddSyncStatusForPath(
                event->stable_id, path, SyncStatus::kInProgress, progress);
            break;
          }
          case mojom::ItemEvent::State::kFailed:
            // This state only comes through for failed downloads of pinned
            // files. Other transfer failures are reported through the OnError()
            // event.
            sync_status_tracker_->AddSyncStatusForPath(event->stable_id, path,
                                                       SyncStatus::kError, -1);
            break;
          case mojom::ItemEvent::State::kCompleted:
            sync_status_tracker_->RemovePath(event->stable_id, path);
            break;
          default:
            break;
        }
      }

      LOG_IF(WARNING, has_invalid_progress)
          << "Drive sync: received at least one item with invalid progress "
             "data.";
    }

    for (auto& observer : host_->observers_) {
      observer.OnSyncingStatusUpdate(*status);
    }
  }

  void OnMirrorSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {
    for (auto& observer : host_->observers_) {
      observer.OnMirrorSyncingStatusUpdate(*status);
    }
  }

  void OnFilesChanged(std::vector<mojom::FileChangePtr> changes) override {
    std::vector<mojom::FileChange> changes_values;
    changes_values.reserve(changes.size());
    for (auto& change : changes) {
      changes_values.emplace_back(std::move(*change));
    }
    for (auto& observer : host_->observers_) {
      observer.OnFilesChanged(changes_values);
    }
  }

  void OnError(mojom::DriveErrorPtr error) override {
    base::FilePath path = host_->GetMountPath();

    // Verify if we have a valid stable_id. It could be invalid because the
    // DriveFs version that reports stable_id for DriveErrors hasn't been
    // uprreved into ChromeOS yet, but it could be due to some actual error.
    if (error->stable_id > 0) {
      if (base::FilePath("/").AppendRelativePath(base::FilePath(error->path),
                                                 &path)) {
        sync_status_tracker_->AddSyncStatusForPath(error->stable_id, path,
                                                   SyncStatus::kError, -1);
      } else {
        LOG(ERROR) << "Failed to make path relative to drive root";
      }
    }

    if (!IsKnownEnumValue(error->type)) {
      return;
    }
    for (auto& observer : host_->observers_) {
      observer.OnError(*error);
    }
  }

  void OnTeamDrivesListReady(
      const std::vector<std::string>& team_drive_ids) override {
    host_->delegate_->GetDriveNotificationManager().AddObserver(this);
    host_->delegate_->GetDriveNotificationManager().UpdateTeamDriveIds(
        std::set<std::string>(team_drive_ids.begin(), team_drive_ids.end()),
        {});
    team_drives_fetched_ = true;
  }

  void OnTeamDriveChanged(const std::string& team_drive_id,
                          CreateOrDelete change_type) override {
    if (!team_drives_fetched_) {
      return;
    }
    std::set<std::string> additions;
    std::set<std::string> removals;
    if (change_type == mojom::DriveFsDelegate::CreateOrDelete::kCreated) {
      additions.insert(team_drive_id);
    } else {
      removals.insert(team_drive_id);
    }
    host_->delegate_->GetDriveNotificationManager().UpdateTeamDriveIds(
        additions, removals);
  }

  void ConnectToExtension(
      mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<mojom::NativeMessagingPort> port,
      mojo::PendingRemote<mojom::NativeMessagingHost> host,
      ConnectToExtensionCallback callback) override {
    std::move(callback).Run(host_->delegate_->ConnectToExtension(
        std::move(params), std::move(port), std::move(host)));
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

  // DriveNotificationObserver overrides:
  void OnNotificationReceived(
      const std::map<std::string, int64_t>& invalidations) override {
    std::vector<mojom::FetchChangeLogOptionsPtr> options;
    options.reserve(invalidations.size());
    for (const auto& invalidation : invalidations) {
      options.emplace_back(absl::in_place, invalidation.second,
                           invalidation.first);
    }
    drivefs_interface()->FetchChangeLog(std::move(options));
  }

  void OnNotificationTimerFired() override {
    drivefs_interface()->FetchAllChangeLogs();
  }

  // Owns |this|.
  DriveFsHost* const host_;

  std::unique_ptr<DriveFsSearch> search_;
  std::unique_ptr<DriveFsHttpClient> http_client_;
  std::unique_ptr<SyncStatusTracker> sync_status_tracker_ = nullptr;

  bool token_fetch_attempted_ = false;
  bool team_drives_fetched_ = false;
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
}

void DriveFsHost::AddObserver(DriveFsHostObserver* observer) {
  observers_.AddObserver(observer);
}

void DriveFsHost::RemoveObserver(DriveFsHostObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool DriveFsHost::Mount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const AccountId& account_id = delegate_->GetAccountId();
  if (mount_state_ || !account_id.HasAccountIdKey() ||
      account_id.GetUserEmail().empty()) {
    return false;
  }
  mount_state_ = std::make_unique<MountState>(this);
  return true;
}

void DriveFsHost::Unmount() {
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

SyncStatusAndProgress DriveFsHost::GetSyncStatusForPath(
    const base::FilePath& drive_path) const {
  if (!mount_state_) {
    return SyncStatusAndProgress::kNotFound;
  }
  return mount_state_->GetSyncStatusForPath(drive_path);
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

}  // namespace drivefs
