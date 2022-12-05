// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"

#include <utility>

#include "base/no_destructor.h"
#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace ash::trash_service {

namespace {

LaunchCallback& GetLaunchOverride() {
  static base::NoDestructor<LaunchCallback> callback;
  return *callback;
}

}  // namespace

mojo::PendingRemote<mojom::TrashService> LaunchTrashService() {
  auto& launcher = GetLaunchOverride();
  if (launcher)
    return launcher.Run();

  mojo::PendingRemote<mojom::TrashService> remote;
  content::ServiceProcessHost::Launch<mojom::TrashService>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("ChromeOS Trash Service")
          .Pass());
  return remote;
}

void SetTrashServiceLaunchOverrideForTesting(LaunchCallback callback) {
  GetLaunchOverride() = std::move(callback);
}

}  // namespace ash::trash_service
