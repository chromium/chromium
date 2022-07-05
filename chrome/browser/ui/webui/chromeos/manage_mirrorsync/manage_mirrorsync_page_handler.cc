// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync_page_handler.h"

#include <vector>

#include "base/files/file.h"

namespace chromeos {

ManageMirrorSyncPageHandler::ManageMirrorSyncPageHandler(
    mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandler>
        pending_page_handler)
    : receiver_{this, std::move(pending_page_handler)} {}

ManageMirrorSyncPageHandler::~ManageMirrorSyncPageHandler() = default;

void ManageMirrorSyncPageHandler::GetSyncingPaths(
    GetSyncingPathsCallback callback) {
  // TODO(b/237066325): Replace this with a call to the DriveIntegrationService
  // to actually get the syncing paths.
  std::vector<base::FilePath> placeholder_paths{base::FilePath("/foo/bar")};
  std::move(callback).Run(std::move(placeholder_paths));
}

}  // namespace chromeos
