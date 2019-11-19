// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/fake_drivefs_launcher_client.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/platform/named_platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "mojo/public/cpp/system/invitation.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "url/gurl.h"

namespace drivefs {
namespace {

void ConnectAsync(mojo::PendingReceiver<mojom::FakeDriveFsLauncher> receiver,
                  mojo::NamedPlatformChannel::ServerName server_name) {
  mojo::PlatformChannelEndpoint endpoint =
      mojo::NamedPlatformChannel::ConnectToServer(server_name);
  if (!endpoint.is_valid())
    return;

  mojo::OutgoingInvitation invitation;
  mojo::FuseMessagePipes(invitation.AttachMessagePipe("drivefs-launcher"),
                         receiver.PassPipe());
  mojo::OutgoingInvitation::Send(std::move(invitation),
                                 base::kNullProcessHandle, std::move(endpoint));
}

}  // namespace

// static
void FakeDriveFsLauncherClient::Init(const base::FilePath& chroot_path,
                                     const base::FilePath& socket_path) {
  DCHECK(!base::SysInfo::IsRunningOnChromeOS());
  DCHECK(chroot_path.IsAbsolute());
  DCHECK(!socket_path.IsAbsolute());

  static base::NoDestructor<FakeDriveFsLauncherClient>
      fake_drivefs_launcher_client(chroot_path, socket_path);
}

FakeDriveFsLauncherClient::FakeDriveFsLauncherClient(
    const base::FilePath& chroot_path,
    const base::FilePath& socket_path)
    : chroot_path_(chroot_path),
      socket_path_(chroot_path_.Append(socket_path)) {
  base::PostTask(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConnectAsync, launcher_.BindNewPipeAndPassReceiver(),
                     socket_path_.value()));

  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  static_cast<chromeos::FakeCrosDisksClient*>(
      dbus_thread_manager->GetCrosDisksClient())
      ->AddCustomMountPointCallback(
          base::BindRepeating(&FakeDriveFsLauncherClient::MaybeMountDriveFs,
                              base::Unretained(this)));
}

FakeDriveFsLauncherClient::~FakeDriveFsLauncherClient() = default;

base::FilePath FakeDriveFsLauncherClient::MaybeMountDriveFs(
    const std::string& source_path,
    const std::vector<std::string>& mount_options) {
  GURL source_url(source_path);
  DCHECK(source_url.is_valid());
  if (source_url.scheme() != "drivefs") {
    return {};
  }
  const auto identity = base::FilePath(source_url.path()).BaseName().value();
  std::string datadir_suffix;
  for (const auto& option : mount_options) {
    if (base::StartsWith(option, "datadir=", base::CompareCase::SENSITIVE)) {
      auto datadir =
          base::FilePath(base::StringPiece(option).substr(strlen("datadir=")));
      CHECK(datadir.IsAbsolute());
      CHECK(!datadir.ReferencesParent());
      datadir_suffix = datadir.BaseName().value();
      break;
    }
  }
  const std::string datadir = base::StrCat({"drivefs-", datadir_suffix});
  mojo::PlatformChannel channel;
  mojo_bootstrap::PendingConnectionManager::Get().OpenIpcChannel(
      identity, channel.TakeLocalEndpoint().TakePlatformHandle().TakeFD());
  launcher_->LaunchDriveFs(
      base::FilePath("/tmp").Append(datadir),
      base::FilePath("/media/fuse").Append(datadir),
      mojo::WrapPlatformHandle(
          channel.TakeRemoteEndpoint().TakePlatformHandle()));
  return chroot_path_.Append("media/fuse").Append(datadir);
}

}  // namespace drivefs
