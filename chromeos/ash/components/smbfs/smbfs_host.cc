// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/smbfs/smbfs_host.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace smbfs {
namespace {

class SmbFsDelegateImpl : public mojom::SmbFsDelegate {
 public:
  SmbFsDelegateImpl(
      mojo::PendingReceiver<mojom::SmbFsDelegate> pending_receiver,
      base::OnceClosure disconnect_callback,
      SmbFsHost::Delegate* delegate)
      : receiver_(this, std::move(pending_receiver)), delegate_(delegate) {
    receiver_.set_disconnect_handler(std::move(disconnect_callback));
  }

  SmbFsDelegateImpl(const SmbFsDelegateImpl&) = delete;
  SmbFsDelegateImpl& operator=(const SmbFsDelegateImpl&) = delete;

  ~SmbFsDelegateImpl() override = default;

  // mojom::SmbFsDelegate overrides.
  void RequestCredentials(RequestCredentialsCallback callback) override {
    delegate_->RequestCredentials(
        base::BindOnce(&SmbFsDelegateImpl::OnRequestCredentialsDone,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

 private:
  void OnRequestCredentialsDone(RequestCredentialsCallback callback,
                                bool cancel,
                                const std::string& username,
                                const std::string& workgroup,
                                const std::string& password) {
    if (cancel) {
      std::move(callback).Run(nullptr);
      return;
    }

    mojom::CredentialsPtr creds =
        mojom::Credentials::New(username, workgroup, nullptr);
    if (password.size() > mojom::Password::kMaxLength) {
      LOG(WARNING) << "smbfs password too long";
    } else if (!password.empty()) {
      // Create pipe and write password.
      base::ScopedFD pipe_read_end;
      base::ScopedFD pipe_write_end;
      CHECK(base::CreatePipe(&pipe_read_end, &pipe_write_end,
                             true /* non_blocking */));
      CHECK(base::WriteFileDescriptor(pipe_write_end.get(), password));

      creds->password = mojom::Password::New(
          mojo::WrapPlatformHandle(
              mojo::PlatformHandle(std::move(pipe_read_end))),
          static_cast<int32_t>(password.size()));
    }
    std::move(callback).Run(std::move(creds));
  }

  mojo::Receiver<mojom::SmbFsDelegate> receiver_;
  const raw_ptr<SmbFsHost::Delegate> delegate_;

  base::WeakPtrFactory<SmbFsDelegateImpl> weak_factory_{this};
};

}  // namespace

SmbFsHost::Delegate::~Delegate() = default;

SmbFsHost::SmbFsHost(
    std::unique_ptr<ash::disks::MountPoint> mount_point,
    Delegate* delegate,
    mojo::Remote<mojom::SmbFs> smbfs_remote,
    mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver)
    : mount_point_(std::move(mount_point)),
      delegate_(delegate),
      smbfs_(std::move(smbfs_remote)),
      delegate_impl_(std::make_unique<SmbFsDelegateImpl>(
          std::move(delegate_receiver),
          base::BindOnce(&SmbFsHost::OnDisconnect, base::Unretained(this)),
          delegate)) {
  DCHECK(mount_point_);
  DCHECK(delegate);

  smbfs_.set_disconnect_handler(
      base::BindOnce(&SmbFsHost::OnDisconnect, base::Unretained(this)));
}

SmbFsHost::~SmbFsHost() = default;

void SmbFsHost::Unmount(SmbFsHost::UnmountCallback callback) {
  mount_point_->Unmount(base::BindOnce(
      &SmbFsHost::OnUnmountDone, base::Unretained(this), std::move(callback)));
}

void SmbFsHost::OnUnmountDone(SmbFsHost::UnmountCallback callback,
                              ash::MountError result) {
  LOG_IF(ERROR, result != ash::MountError::kSuccess)
      << "Could not unmount smbfs share: " << result;
  std::move(callback).Run(result);
}

void SmbFsHost::RemoveSavedCredentials(
    SmbFsHost::RemoveSavedCredentialsCallback callback) {
  smbfs_->RemoveSavedCredentials(
      base::BindOnce(&SmbFsHost::OnRemoveSavedCredentialsDone,
                     base::Unretained(this), std::move(callback)));
}

void SmbFsHost::OnRemoveSavedCredentialsDone(
    SmbFsHost::RemoveSavedCredentialsCallback callback,
    bool success) {
  LOG_IF(ERROR, !success) << "Unable to remove saved password for smbfs";
  std::move(callback).Run(success);
}

void SmbFsHost::OnDisconnect() {
  // Ensure only one disconnection event occurs.
  smbfs_.reset();
  delegate_impl_.reset();

  // This may delete us.
  delegate_->OnDisconnected();
}

void SmbFsHost::DeleteRecursively(const base::FilePath& path,
                                  DeleteRecursivelyCallback callback) {
  smbfs_->DeleteRecursively(
      path, base::BindOnce(&SmbFsHost::OnDeleteRecursivelyDone,
                           base::Unretained(this), std::move(callback)));
}

void SmbFsHost::OnDeleteRecursivelyDone(
    DeleteRecursivelyCallback callback,
    smbfs::mojom::DeleteRecursivelyError error) {
  base::File::Error file_error =
      error == smbfs::mojom::DeleteRecursivelyError::kOk
          ? base::File::FILE_OK
          : base::File::FILE_ERROR_FAILED;

  std::move(callback).Run(file_error);
}

}  // namespace smbfs
