// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trash_service/public/cpp/trash_service.h"

#include "chromeos/ash/components/trash_service/public/mojom/trash_service.mojom.h"
#include "content/public/browser/service_process_host.h"

namespace chromeos::trash_service {

mojo::PendingRemote<mojom::TrashService> LaunchTrashService() {
  mojo::PendingRemote<mojom::TrashService> remote;
  content::ServiceProcessHost::Launch<mojom::TrashService>(
      remote.InitWithNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("ChromeOS Trash Service")
          .Pass());
  return remote;
}

}  // namespace chromeos::trash_service
