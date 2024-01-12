// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_SMBFS_SMBFS_MOUNTER_H_
#define CHROMEOS_ASH_COMPONENTS_SMBFS_SMBFS_MOUNTER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/disks/disk_mount_manager.h"
#include "chromeos/ash/components/disks/mount_point.h"
#include "chromeos/ash/components/smbfs/mojom/smbfs.mojom.h"
#include "chromeos/ash/components/smbfs/smbfs_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/invitation.h"
#include "net/base/ip_address.h"

namespace smbfs {

// SmbFsMounter is a helper class that is used to mount an instance of smbfs. It
// performs all the actions necessary to start smbfs and initiate a connection
// to the SMB server.
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_SMBFS) SmbFsMounter {
 public:
  using DoneCallback =
      base::OnceCallback<void(mojom::MountError, std::unique_ptr<SmbFsHost>)>;

  struct KerberosOptions {
    using Source = mojom::KerberosConfig::Source;
    KerberosOptions(Source source, const std::string& identity);
    ~KerberosOptions();

    // Don't allow an invalid options struct to be created.
    KerberosOptions() = delete;

    Source source;
    std::string identity;
  };

  struct MountOptions {
    MountOptions();
    MountOptions(const MountOptions&);
    ~MountOptions();

    // Resolved IP address for share's hostname.
    net::IPAddress resolved_host;

    // Authentication options.
    std::string username;
    std::string workgroup;
    std::string password;
    std::optional<KerberosOptions> kerberos_options;

    // Allow NTLM authentication to be used.
    bool allow_ntlm = false;

    // Skip attempting to connect to the share.
    bool skip_connect = false;

    // Run /usr/sbin/smbfs with a chattier log-level.
    bool enable_verbose_logging = false;

    // Have smbfs save/restore the share's password.
    bool save_restore_password = false;
    std::string account_hash;
    std::vector<uint8_t> password_salt;
  };

  SmbFsMounter(const std::string& share_path,
               const std::string& mount_dir_name,
               const MountOptions& options,
               SmbFsHost::Delegate* delegate,
               ash::disks::DiskMountManager* disk_mount_manager);

  SmbFsMounter(const SmbFsMounter&) = delete;
  SmbFsMounter& operator=(const SmbFsMounter&) = delete;

  virtual ~SmbFsMounter();

  // Initiate the filesystem mount request, and run |callback| when completed.
  // |callback| is guaranteed not to run after |this| is destroyed.
  // Must only be called once. Virtual for testing.
  virtual void Mount(DoneCallback callback);

 protected:
  // Additional constructors for tests.
  SmbFsMounter();
  SmbFsMounter(const std::string& share_path,
               const std::string& mount_dir_name,
               const MountOptions& options,
               SmbFsHost::Delegate* delegate,
               ash::disks::DiskMountManager* disk_mount_manager,
               mojo::Remote<mojom::SmbFsBootstrap> bootstrap);

 private:
  // Callback for MountPoint::Mount().
  void OnMountDone(ash::MountError error_code,
                   std::unique_ptr<ash::disks::MountPoint> mount_point);

  // Callback for receiving a Mojo bootstrap channel.
  void OnIpcChannel(base::ScopedFD mojo_fd);

  // Callback for bootstrap Mojo MountShare() method.
  void OnMountShare(
      mojo::PendingReceiver<mojom::SmbFsDelegate> delegate_receiver,
      mojom::MountError mount_error,
      mojo::PendingRemote<mojom::SmbFs> smbfs);

  // Mojo disconnection handler.
  void OnMojoDisconnect();

  // Mount timeout handler.
  void OnMountTimeout();

  // Perform cleanup and run |callback_| with |mount_error|.
  void ProcessMountError(mojom::MountError mount_error);

  const std::string share_path_;
  const std::string mount_dir_name_;
  const MountOptions options_;
  const raw_ptr<SmbFsHost::Delegate> delegate_;
  const raw_ptr<ash::disks::DiskMountManager> disk_mount_manager_;
  const base::UnguessableToken token_;
  const std::string mount_url_;
  bool mojo_fd_pending_ = false;

  base::OneShotTimer mount_timer_;
  DoneCallback callback_;

  std::unique_ptr<ash::disks::MountPoint> mount_point_;
  mojo::OutgoingInvitation bootstrap_invitation_;
  mojo::Remote<mojom::SmbFsBootstrap> bootstrap_;

  base::WeakPtrFactory<SmbFsMounter> weak_factory_{this};
};

}  // namespace smbfs

#endif  // CHROMEOS_ASH_COMPONENTS_SMBFS_SMBFS_MOUNTER_H_
