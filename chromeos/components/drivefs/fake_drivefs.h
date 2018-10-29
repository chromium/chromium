// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_H_
#define CHROMEOS_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/drivefs/drivefs_host.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace drivefs {

class FakeDriveFs : public drivefs::mojom::DriveFs,
                    public drivefs::mojom::DriveFsBootstrap {
 public:
  explicit FakeDriveFs(const base::FilePath& mount_path);
  ~FakeDriveFs() override;

  void RegisterMountingForAccountId(
      base::RepeatingCallback<std::string()> account_id_getter);

  std::unique_ptr<drivefs::DriveFsHost::MojoConnectionDelegate>
  CreateConnectionDelegate();

  void SetMetadata(const base::FilePath& path,
                   const std::string& mime_type,
                   const std::string& original_name,
                   bool pinned,
                   bool shared,
                   const mojom::Capabilities& capabilities);

  const base::FilePath& mount_path() { return mount_path_; }

 private:
  struct FileMetadata;
  class SearchQuery;

  // drivefs::mojom::DriveFsBootstrap:
  void Init(drivefs::mojom::DriveFsConfigurationPtr config,
            drivefs::mojom::DriveFsRequest drive_fs_request,
            drivefs::mojom::DriveFsDelegatePtr delegate) override;

  // drivefs::mojom::DriveFs:
  void GetMetadata(const base::FilePath& path,
                   GetMetadataCallback callback) override;

  void SetPinned(const base::FilePath& path,
                 bool pinned,
                 SetPinnedCallback callback) override;

  void UpdateNetworkState(bool pause_syncing, bool is_offline) override;

  void ResetCache(ResetCacheCallback callback) override;

  void GetThumbnail(const base::FilePath& path,
                    bool crop_to_square,
                    GetThumbnailCallback callback) override;

  void CopyFile(const base::FilePath& source,
                const base::FilePath& target,
                CopyFileCallback callback) override;

  void StartSearchQuery(
      drivefs::mojom::SearchQueryRequest query,
      drivefs::mojom::QueryParametersPtr query_params) override;

  void FetchAllChangeLogs() override;

  void FetchChangeLog(
      std::vector<mojom::FetchChangeLogOptionsPtr> options) override;

  const base::FilePath mount_path_;

  std::map<base::FilePath, FileMetadata> metadata_;

  mojo::Binding<drivefs::mojom::DriveFs> binding_;
  drivefs::mojom::DriveFsDelegatePtr delegate_;
  mojo::Binding<drivefs::mojom::DriveFsBootstrap> bootstrap_binding_;
  drivefs::mojom::DriveFsDelegateRequest pending_delegate_request_;

  base::WeakPtrFactory<FakeDriveFs> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveFs);
};

}  // namespace drivefs

#endif  // CHROMEOS_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_H_
