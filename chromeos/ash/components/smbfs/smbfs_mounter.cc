// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/smbfs/smbfs_mounter.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace smbfs {

namespace {
constexpr char kMountUrlPrefix[] = "smbfs://";
constexpr base::TimeDelta kMountTimeout = base::Seconds(20);
}  // namespace

SmbFsMounter::KerberosOptions::KerberosOptions(Source source,
                                               const std::string& identity)
    : source(source), identity(identity) {
  DCHECK(source == Source::kActiveDirectory || source == Source::kKerberos);
  DCHECK(!identity.empty());
}

SmbFsMounter::KerberosOptions::~KerberosOptions() = default;

SmbFsMounter::MountOptions::MountOptions() = default;

SmbFsMounter::MountOptions::MountOptions(const MountOptions&) = default;

SmbFsMounter::MountOptions::~MountOptions() = default;

SmbFsMounter::SmbFsMounter(const std::string& share_path,
                           const std::string& mount_dir_name,
                           const MountOptions& options,
                           SmbFsHost::Delegate* delegate,
                           ash::disks::DiskMountManager* disk_mount_manager)
    : SmbFsMounter(share_path,
                   mount_dir_name,
                   options,
                   delegate,
                   disk_mount_manager,
                   {}) {}

SmbFsMounter::SmbFsMounter(const std::string& share_path,
                           const std::string& mount_dir_name,
                           const MountOptions& options,
                           SmbFsHost::Delegate* delegate,
                           ash::disks::DiskMountManager* disk_mount_manager,
                           mojo::Remote<mojom::SmbFsBootstrap> bootstrap)
    : share_path_(share_path),
      mount_dir_name_(mount_dir_name),
      options_(options),
      delegate_(delegate),
      disk_mount_manager_(disk_mount_manager),
      token_(base::UnguessableToken::Create()),
      mount_url_(base::StrCat({kMountUrlPrefix, token_.ToString()})),
      bootstrap_(std::move(bootstrap)) {
  DCHECK(delegate_);
  DCHECK(disk_mount_manager_);
}

SmbFsMounter::SmbFsMounter()
    : delegate_(nullptr), disk_mount_manager_(nullptr) {}

SmbFsMounter::~SmbFsMounter() {
  if (mojo_fd_pending_) {
    mojo_bootstrap::PendingConnectionManager::Get()
        .CancelExpectedOpenIpcChannel(token_);
  }
}

void SmbFsMounter::Mount(SmbFsMounter::DoneCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  CHECK(!mojo_fd_pending_);

  callback_ = std::move(callback);

  // If |bootstrap_| is already bound, it was provided by a test subclass.
  if (!bootstrap_) {
    mojo_bootstrap::PendingConnectionManager::Get().ExpectOpenIpcChannel(
        token_,
        base::BindOnce(&SmbFsMounter::OnIpcChannel, base::Unretained(this)));
    mojo_fd_pending_ = true;

    bootstrap_.Bind(mojo::PendingRemote<mojom::SmbFsBootstrap>(
        bootstrap_invitation_.AttachMessagePipe(mojom::kBootstrapPipeName),
        mojom::SmbFsBootstrap::Version_));
  }
  bootstrap_.set_disconnect_handler(
      base::BindOnce(&SmbFsMounter::OnMojoDisconnect, base::Unretained(this)));

  std::vector<std::string> mount_options;
  if (options_.enable_verbose_logging) {
    mount_options.emplace_back("log-level=-2");
  }

  ash::disks::MountPoint::Mount(
      disk_mount_manager_, mount_url_, "" /* source_format */, mount_dir_name_,
      mount_options, ash::MountType::kNetworkStorage,
      ash::MountAccessMode::kReadWrite,
      base::BindOnce(&SmbFsMounter::OnMountDone, weak_factory_.GetWeakPtr()));
  mount_timer_.Start(
      FROM_HERE, kMountTimeout,
      base::BindOnce(&SmbFsMounter::OnMountTimeout, base::Unretained(this)));
}

void SmbFsMounter::OnMountDone(
    ash::MountError error_code,
    std::unique_ptr<ash::disks::MountPoint> mount_point) {
  if (!callback_) {
    // This can happen if the mount timeout expires and the callback is already
    // run with a timeout error.
    return;
  }

  if (error_code != ash::MountError::kSuccess) {
    LOG(WARNING) << "smbfs mount error: " << error_code;
    ProcessMountError(mojom::MountError::kUnknown);
    return;
  }

  DCHECK(mount_point);
  mount_point_ = std::move(mount_point);

  mojom::MountOptionsPtr mount_options = mojom::MountOptions::New();
  mount_options->share_path = share_path_;
  if (options_.resolved_host.IsIPv4()) {
    // TODO(crbug.com/1051291): Support IPv6.
    mount_options->resolved_host = options_.resolved_host;
  }
  mount_options->username = options_.username;
  mount_options->workgroup = options_.workgroup;
  mount_options->allow_ntlm = options_.allow_ntlm;
  mount_options->skip_connect = options_.skip_connect;

  if (options_.save_restore_password) {
    DCHECK_GE(
        options_.password_salt.size(),
        static_cast<size_t>(mojom::CredentialStorageOptions::kMinSaltLength));
    mount_options->credential_storage_options =
        mojom::CredentialStorageOptions::New(options_.account_hash,
                                             options_.password_salt);
  }

  if (options_.kerberos_options) {
    mojom::KerberosConfigPtr kerberos_config = mojom::KerberosConfig::New();
    kerberos_config->source = options_.kerberos_options->source;
    kerberos_config->identity = options_.kerberos_options->identity;
    mount_options->kerberos_config = std::move(kerberos_config);
  } else if (!options_.password.empty()) {
    if (options_.password.size() > mojom::Password::kMaxLength) {
      LOG(WARNING) << "smbfs password too long";
      ProcessMountError(mojom::MountError::kUnknown);
      return;
    }
    int pipe_fds[2];
    CHECK(base::CreateLocalNonBlockingPipe(pipe_fds));
    base::ScopedFD pipe_read_end(pipe_fds[0]);
    base::ScopedFD pipe_write_end(pipe_fds[1]);
    // Write password to pipe.
    CHECK(base::WriteFileDescriptor(pipe_write_end.get(), options_.password));

    mojom::PasswordPtr password = mojom::Password::New();
    password->length = static_cast<int32_t>(options_.password.size());
    password->fd = mojo::WrapPlatformHandle(
        mojo::PlatformHandle(std::move(pipe_read_end)));
    mount_options->password = std::move(password);
  }

  mojo::PendingRemote<mojom::SmbFsDelegate> delegate_remote;
  mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver =
      delegate_remote.InitWithNewPipeAndPassReceiver();

  bootstrap_->MountShare(
      std::move(mount_options), std::move(delegate_remote),
      base::BindOnce(&SmbFsMounter::OnMountShare, base::Unretained(this),
                     std::move(delegate_receiver)));
}

void SmbFsMounter::OnIpcChannel(base::ScopedFD mojo_fd) {
  DCHECK(mojo_fd.is_valid());
  mojo::OutgoingInvitation::Send(
      std::move(bootstrap_invitation_), base::kNullProcessHandle,
      mojo::PlatformChannelEndpoint(mojo::PlatformHandle(std::move(mojo_fd))));
  mojo_fd_pending_ = false;
}

void SmbFsMounter::OnMountShare(
    mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver,
    mojom::MountError mount_error,
    mojo::PendingRemote<mojom::SmbFs> smbfs) {
  if (!callback_) {
    return;
  }

  if (mount_error != mojom::MountError::kOk) {
    LOG(WARNING) << "smbfs mount share error: " << mount_error;
    ProcessMountError(mount_error);
    return;
  }

  DCHECK(mount_point_);
  std::unique_ptr<SmbFsHost> host =
      std::make_unique<SmbFsHost>(std::move(mount_point_), delegate_,
                                  mojo::Remote<mojom::SmbFs>(std::move(smbfs)),
                                  std::move(delegate_receiver));
  std::move(callback_).Run(mojom::MountError::kOk, std::move(host));
}

void SmbFsMounter::OnMojoDisconnect() {
  if (!callback_) {
    return;
  }

  LOG(WARNING) << "smbfs bootstrap disconnection";
  ProcessMountError(mojom::MountError::kUnknown);
}

void SmbFsMounter::OnMountTimeout() {
  if (!callback_) {
    return;
  }

  LOG(ERROR) << "smbfs mount timeout";
  ProcessMountError(mojom::MountError::kTimeout);
}

void SmbFsMounter::ProcessMountError(mojom::MountError mount_error) {
  mount_point_.reset();
  std::move(callback_).Run(mount_error, nullptr);
}

}  // namespace smbfs
