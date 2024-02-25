// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/fake_drivefs_launcher_client.h"

#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"
#include "base/task/thread_pool.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/cros_disks/fake_cros_disks_client.h"
#include "chromeos/components/mojo_bootstrap/pending_connection_manager.h"
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
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ConnectAsync, launcher_.BindNewPipeAndPassReceiver(),
                     socket_path_.value()));

  static_cast<ash::FakeCrosDisksClient*>(ash::CrosDisksClient::Get())
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
          base::FilePath(std::string_view(option).substr(strlen("datadir=")));
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
