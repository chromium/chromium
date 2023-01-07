// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_PAGE_HANDLER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace ash {

// Handles communication from the chrome://manage-mirrorsync renderer process to
// the browser process exposing various methods for the JS to invoke.
class ManageMirrorSyncPageHandler
    : public manage_mirrorsync::mojom::PageHandler {
 public:
  ManageMirrorSyncPageHandler(
      mojo::PendingReceiver<manage_mirrorsync::mojom::PageHandler>
          pending_page_handler,
      Profile* profile);

  ManageMirrorSyncPageHandler(const ManageMirrorSyncPageHandler&) = delete;
  ManageMirrorSyncPageHandler& operator=(const ManageMirrorSyncPageHandler&) =
      delete;

  ~ManageMirrorSyncPageHandler() override;

  // manage_mirrorsync::mojom::PageHandler:
  void GetChildFolders(const base::FilePath& path,
                       GetChildFoldersCallback callback) override;

  void GetSyncingPaths(GetSyncingPathsCallback callback) override;

 private:
  void OnDirectoryExists(const base::FilePath& absolute_path,
                         GetChildFoldersCallback callback,
                         bool exists);

  void OnGetChildFolders(GetChildFoldersCallback callback,
                         std::vector<base::FilePath> child_folders);

  void OnGetSyncingPaths(GetSyncingPathsCallback callback,
                         drive::FileError error,
                         const std::vector<base::FilePath>& syncing_paths);

  raw_ptr<Profile> profile_;
  const base::FilePath my_files_dir_;
  mojo::Receiver<manage_mirrorsync::mojom::PageHandler> receiver_;
  base::WeakPtrFactory<ManageMirrorSyncPageHandler> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_MANAGE_MIRRORSYNC_MANAGE_MIRRORSYNC_PAGE_HANDLER_H_
