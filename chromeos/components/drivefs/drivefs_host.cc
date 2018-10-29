// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/drivefs_host.h"

#include <map>
#include <set>
#include <utility>

#include "base/strings/strcat.h"
#include "base/unguessable_token.h"
#include "chromeos/components/drivefs/drivefs_host_observer.h"
#include "chromeos/components/drivefs/pending_connection_manager.h"
#include "components/drive/drive_notification_manager.h"
#include "components/drive/drive_notification_observer.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/service_manager/public/cpp/connector.h"

namespace drivefs {

namespace {

constexpr char kMountScheme[] = "drivefs://";
constexpr char kDataPath[] = "GCache/v2";
constexpr char kIdentityConsumerId[] = "drivefs";
constexpr base::TimeDelta kMountTimeout = base::TimeDelta::FromMinutes(1);

class MojoConnectionDelegateImpl : public DriveFsHost::MojoConnectionDelegate {
 public:
  MojoConnectionDelegateImpl() = default;

  mojom::DriveFsBootstrapPtrInfo InitializeMojoConnection() override {
    return mojom::DriveFsBootstrapPtrInfo(
        invitation_.AttachMessagePipe("drivefs-bootstrap"),
        mojom::DriveFsBootstrap::Version_);
  }

  void AcceptMojoConnection(base::ScopedFD handle) override {
    mojo::OutgoingInvitation::Send(
        std::move(invitation_), base::kNullProcessHandle,
        mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(handle))));
  }

 private:
  // The underlying mojo connection.
  mojo::OutgoingInvitation invitation_;

  DISALLOW_COPY_AND_ASSIGN(MojoConnectionDelegateImpl);
};

}  // namespace

std::unique_ptr<OAuth2MintTokenFlow> DriveFsHost::Delegate::CreateMintTokenFlow(
    OAuth2MintTokenFlow::Delegate* delegate,
    const std::string& client_id,
    const std::string& app_id,
    const std::vector<std::string>& scopes) {
  return std::make_unique<OAuth2MintTokenFlow>(
      delegate, OAuth2MintTokenFlow::Parameters{
                    app_id, client_id, scopes, kIdentityConsumerId,
                    OAuth2MintTokenFlow::MODE_MINT_TOKEN_FORCE});
}

std::unique_ptr<DriveFsHost::MojoConnectionDelegate>
DriveFsHost::Delegate::CreateMojoConnectionDelegate() {
  return std::make_unique<MojoConnectionDelegateImpl>();
}

class DriveFsHost::AccountTokenDelegate : public OAuth2MintTokenFlow::Delegate {
 public:
  explicit AccountTokenDelegate(DriveFsHost* host) : host_(host) {}
  ~AccountTokenDelegate() override = default;

  void GetAccessToken(bool use_cached,
                      const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      mojom::DriveFsDelegate::GetAccessTokenCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (get_access_token_callback_) {
      std::move(callback).Run(mojom::AccessTokenStatus::kTransientError, "");
      return;
    }
    DCHECK(!mint_token_flow_);
    const std::string& token =
        MaybeGetCachedToken(use_cached, client_id, app_id, scopes);
    if (!token.empty()) {
      std::move(callback).Run(mojom::AccessTokenStatus::kSuccess, token);
      return;
    }
    get_access_token_callback_ = std::move(callback);
    mint_token_flow_ =
        host_->delegate_->CreateMintTokenFlow(this, client_id, app_id, scopes);
    DCHECK(mint_token_flow_);
    GetIdentityManager().GetPrimaryAccountWhenAvailable(base::BindOnce(
        &AccountTokenDelegate::AccountReady, base::Unretained(this)));
  }

 private:
  void AccountReady(const AccountInfo& info,
                    const identity::AccountState& state) {
    GetIdentityManager().GetAccessToken(
        host_->delegate_->GetAccountId().GetUserEmail(), {},
        kIdentityConsumerId,
        base::BindOnce(&AccountTokenDelegate::GotChromeAccessToken,
                       base::Unretained(this)));
  }

  void GotChromeAccessToken(const base::Optional<std::string>& access_token,
                            base::Time expiration_time,
                            const GoogleServiceAuthError& error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (!access_token) {
      OnMintTokenFailure(error);
      return;
    }
    mint_token_flow_->Start(host_->delegate_->GetURLLoaderFactory(),
                            *access_token);
  }

  // OAuth2MintTokenFlow::Delegate:
  void OnMintTokenSuccess(const std::string& access_token,
                          int time_to_live) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    UpdateCachedToken(access_token, base::TimeDelta::FromSeconds(time_to_live));
    std::move(get_access_token_callback_)
        .Run(mojom::AccessTokenStatus::kSuccess, access_token);
    mint_token_flow_.reset();
  }

  void OnMintTokenFailure(const GoogleServiceAuthError& error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    std::move(get_access_token_callback_)
        .Run(error.IsPersistentError()
                 ? mojom::AccessTokenStatus::kAuthError
                 : mojom::AccessTokenStatus::kTransientError,
             "");
    mint_token_flow_.reset();
  }

  const std::string& MaybeGetCachedToken(
      bool use_cached,
      const std::string& client_id,
      const std::string& app_id,
      const std::vector<std::string>& scopes) {
    // Return value from cache at most once per mount.
    if (!use_cached || host_->clock_->Now() >= last_token_expiry_) {
      last_token_.clear();
    }
    return last_token_;
  }

  void UpdateCachedToken(const std::string& token, const base::TimeDelta& ttl) {
    last_token_ = token;
    last_token_expiry_ = host_->clock_->Now() + ttl;
  }

  identity::mojom::IdentityManager& GetIdentityManager() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (!identity_manager_) {
      host_->delegate_->GetConnector()->BindInterface(
          identity::mojom::kServiceName, mojo::MakeRequest(&identity_manager_));
    }
    return *identity_manager_;
  }

  // Owns |this|.
  DriveFsHost* const host_;

  // The connection to the identity service. Access via |GetIdentityManager()|.
  identity::mojom::IdentityManagerPtr identity_manager_;

  // Pending callback for an in-flight GetAccessToken request.
  mojom::DriveFsDelegate::GetAccessTokenCallback get_access_token_callback_;

  // The mint token flow, if one is in flight.
  std::unique_ptr<OAuth2MintTokenFlow> mint_token_flow_;

  std::string last_token_;
  base::Time last_token_expiry_;

  DISALLOW_COPY_AND_ASSIGN(AccountTokenDelegate);
};

// A container of state tied to a particular mounting of DriveFS. None of this
// should be shared between mounts.
class DriveFsHost::MountState
    : public mojom::DriveFsDelegate,
      public OAuth2MintTokenFlow::Delegate,
      public chromeos::disks::DiskMountManager::Observer,
      public drive::DriveNotificationObserver {
 public:
  explicit MountState(DriveFsHost* host)
      : host_(host),
        mojo_connection_delegate_(
            host_->delegate_->CreateMojoConnectionDelegate()),
        pending_token_(base::UnguessableToken::Create()),
        binding_(this) {
    host_->disk_mount_manager_->AddObserver(this);
    source_path_ = base::StrCat({kMountScheme, pending_token_.ToString()});
    std::string datadir_option =
        base::StrCat({"datadir=", host_->GetDataPath().value()});
    auto bootstrap =
        mojo::MakeProxy(mojo_connection_delegate_->InitializeMojoConnection());
    mojom::DriveFsDelegatePtr delegate;
    binding_.Bind(mojo::MakeRequest(&delegate));
    binding_.set_connection_error_handler(
        base::BindOnce(&MountState::OnConnectionError, base::Unretained(this)));
    bootstrap->Init(
        {base::in_place, host_->delegate_->GetAccountId().GetUserEmail()},
        mojo::MakeRequest(&drivefs_), std::move(delegate));
    drivefs_.set_connection_error_handler(
        base::BindOnce(&MountState::OnConnectionError, base::Unretained(this)));

    // If unconsumed, the registration is cleaned up when |this| is destructed.
    PendingConnectionManager::Get().ExpectOpenIpcChannel(
        pending_token_,
        base::BindOnce(&DriveFsHost::MountState::AcceptMojoConnection,
                       base::Unretained(this)));

    host_->disk_mount_manager_->MountPath(
        source_path_, "",
        base::StrCat({"drivefs-", host_->delegate_->GetObfuscatedAccountId()}),
        {datadir_option}, chromeos::MOUNT_TYPE_NETWORK_STORAGE,
        chromeos::MOUNT_ACCESS_MODE_READ_WRITE);

    host_->timer_->Start(
        FROM_HERE, kMountTimeout,
        base::BindOnce(&MountState::OnTimedOut, base::Unretained(this)));
  }

  ~MountState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->disk_mount_manager_->RemoveObserver(this);
    host_->delegate_->GetDriveNotificationManager().RemoveObserver(this);
    host_->timer_->Stop();
    if (pending_token_) {
      PendingConnectionManager::Get().CancelExpectedOpenIpcChannel(
          pending_token_);
    }
    if (!mount_path_.empty()) {
      host_->disk_mount_manager_->UnmountPath(
          mount_path_.value(), chromeos::UNMOUNT_OPTIONS_NONE, {});
    }
    if (mounted()) {
      for (auto& observer : host_->observers_) {
        observer.OnUnmounted();
      }
    }
  }

  bool mounted() const { return drivefs_has_mounted_ && !mount_path_.empty(); }
  const base::FilePath& mount_path() const { return mount_path_; }

  mojom::DriveFs* GetDriveFsInterface() const { return drivefs_.get(); }

  // Accepts the mojo connection over |handle|, delegating to
  // |mojo_connection_delegate_|.
  void AcceptMojoConnection(base::ScopedFD handle) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    DCHECK(pending_token_);
    pending_token_ = {};
    mojo_connection_delegate_->AcceptMojoConnection(std::move(handle));
  }

 private:
  // mojom::DriveFsDelegate:
  void GetAccessToken(const std::string& client_id,
                      const std::string& app_id,
                      const std::vector<std::string>& scopes,
                      GetAccessTokenCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->account_token_delegate_->GetAccessToken(!token_fetch_attempted_,
                                                   client_id, app_id, scopes,
                                                   std::move(callback));
    token_fetch_attempted_ = true;
  }

  void OnMounted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    drivefs_has_mounted_ = true;
    MaybeNotifyDelegateOnMounted();
  }

  void OnMountFailed(base::Optional<base::TimeDelta> remount_delay) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    drivefs_has_mounted_ = false;
    drivefs_has_terminated_ = true;
    bool needs_restart = remount_delay.has_value();
    host_->mount_observer_->OnMountFailed(
        needs_restart ? MountObserver::MountFailure::kNeedsRestart
                      : MountObserver::MountFailure::kUnknown,
        std::move(remount_delay));
  }

  void OnUnmounted(base::Optional<base::TimeDelta> remount_delay) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    drivefs_has_mounted_ = false;
    drivefs_has_terminated_ = true;
    host_->mount_observer_->OnUnmounted(std::move(remount_delay));
  }

  void OnSyncingStatusUpdate(mojom::SyncingStatusPtr status) override {
    for (auto& observer : host_->observers_) {
      observer.OnSyncingStatusUpdate(*status);
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
  }

  void OnTeamDriveChanged(const std::string& team_drive_id,
                          CreateOrDelete change_type) override {
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

  void OnConnectionError() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (!drivefs_has_terminated_) {
      if (mounted()) {
        host_->mount_observer_->OnUnmounted({});
      } else {
        host_->mount_observer_->OnMountFailed(
            MountObserver::MountFailure::kIpcDisconnect, {});
      }
      drivefs_has_terminated_ = true;
    }
  }

  void OnTimedOut() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    host_->timer_->Stop();
    drivefs_has_mounted_ = false;
    drivefs_has_terminated_ = true;
    host_->mount_observer_->OnMountFailed(MountObserver::MountFailure::kTimeout,
                                          {});
  }

  void MaybeNotifyDelegateOnMounted() {
    if (mounted()) {
      host_->timer_->Stop();
      host_->mount_observer_->OnMounted(mount_path());
    }
  }

  // DiskMountManager::Observer:
  void OnMountEvent(chromeos::disks::DiskMountManager::MountEvent event,
                    chromeos::MountError error_code,
                    const chromeos::disks::DiskMountManager::MountPointInfo&
                        mount_info) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(host_->sequence_checker_);
    if (mount_info.mount_type != chromeos::MOUNT_TYPE_NETWORK_STORAGE ||
        mount_info.source_path != source_path_ ||
        event != chromeos::disks::DiskMountManager::MOUNTING) {
      return;
    }
    if (error_code != chromeos::MOUNT_ERROR_NONE) {
      auto* observer = host_->mount_observer_;

      // Deletes |this|.
      host_->Unmount();
      observer->OnMountFailed(MountObserver::MountFailure::kInvocation, {});
      return;
    }
    DCHECK(!mount_info.mount_path.empty());
    mount_path_ = base::FilePath(mount_info.mount_path);
    MaybeNotifyDelegateOnMounted();
  }

  // DriveNotificationObserver overrides:
  void OnNotificationReceived(
      const std::map<std::string, int64_t>& invalidations) override {
    std::vector<mojom::FetchChangeLogOptionsPtr> options;
    options.reserve(invalidations.size());
    for (const auto& invalidation : invalidations) {
      options.emplace_back(base::in_place, invalidation.second,
                           invalidation.first);
    }
    drivefs_->FetchChangeLog(std::move(options));
  }

  void OnNotificationTimerFired() override { drivefs_->FetchAllChangeLogs(); }

  // Owns |this|.
  DriveFsHost* const host_;

  const std::unique_ptr<DriveFsHost::MojoConnectionDelegate>
      mojo_connection_delegate_;

  // The token passed to DriveFS as part of |source_path_| used to match it to
  // this DriveFsHost instance.
  base::UnguessableToken pending_token_;

  // The path passed to cros-disks to mount.
  std::string source_path_;

  // The path where DriveFS is mounted.
  base::FilePath mount_path_;

  // Mojo connections to the DriveFS process.
  mojom::DriveFsPtr drivefs_;
  mojo::Binding<mojom::DriveFsDelegate> binding_;

  bool drivefs_has_mounted_ = false;
  bool drivefs_has_terminated_ = false;
  bool token_fetch_attempted_ = false;

  DISALLOW_COPY_AND_ASSIGN(MountState);
};

DriveFsHost::DriveFsHost(const base::FilePath& profile_path,
                         DriveFsHost::Delegate* delegate,
                         DriveFsHost::MountObserver* mount_observer,
                         const base::Clock* clock,
                         chromeos::disks::DiskMountManager* disk_mount_manager,
                         std::unique_ptr<base::OneShotTimer> timer)
    : profile_path_(profile_path),
      delegate_(delegate),
      mount_observer_(mount_observer),
      clock_(clock),
      disk_mount_manager_(disk_mount_manager),
      timer_(std::move(timer)),
      account_token_delegate_(std::make_unique<AccountTokenDelegate>(this)) {
  DCHECK(delegate_);
  DCHECK(mount_observer_);
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
  return mount_state_ && mount_state_->mounted();
}

base::FilePath DriveFsHost::GetMountPath() const {
  return mount_state_ && mount_state_->mounted()
             ? mount_state_->mount_path()
             : base::FilePath("/media/fuse")
                   .Append(base::StrCat(
                       {"drivefs-", delegate_->GetObfuscatedAccountId()}));
}

base::FilePath DriveFsHost::GetDataPath() const {
  return profile_path_.Append(kDataPath).Append(
      delegate_->GetObfuscatedAccountId());
}

mojom::DriveFs* DriveFsHost::GetDriveFsInterface() const {
  if (!mount_state_ || !mount_state_->mounted()) {
    return nullptr;
  }
  return mount_state_->GetDriveFsInterface();
}

}  // namespace drivefs
