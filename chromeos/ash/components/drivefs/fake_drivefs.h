// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/drivefs/drivefs_bootstrap.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace drivefs {

class FakeDriveFsBootstrapListener : public DriveFsBootstrapListener {
 public:
  explicit FakeDriveFsBootstrapListener(
      mojo::PendingRemote<drivefs::mojom::DriveFsBootstrap> bootstrap);

  FakeDriveFsBootstrapListener(const FakeDriveFsBootstrapListener&) = delete;
  FakeDriveFsBootstrapListener& operator=(const FakeDriveFsBootstrapListener&) =
      delete;

  ~FakeDriveFsBootstrapListener() override;

 private:
  void SendInvitationOverPipe(base::ScopedFD) override;
  mojo::PendingRemote<mojom::DriveFsBootstrap> bootstrap() override;

  mojo::PendingRemote<drivefs::mojom::DriveFsBootstrap> bootstrap_;
};

class FakeDriveFs : public drivefs::mojom::DriveFs,
                    public drivefs::mojom::DriveFsBootstrap {
 public:
  explicit FakeDriveFs(const base::FilePath& mount_path);

  FakeDriveFs(const FakeDriveFs&) = delete;
  FakeDriveFs& operator=(const FakeDriveFs&) = delete;

  ~FakeDriveFs() override;

  void RegisterMountingForAccountId(
      base::RepeatingCallback<std::string()> account_id_getter);

  std::unique_ptr<drivefs::DriveFsBootstrapListener> CreateMojoListener();

  void SetMetadata(const base::FilePath& path,
                   const std::string& mime_type,
                   const std::string& original_name,
                   bool pinned,
                   bool available_offline,
                   bool shared,
                   const mojom::Capabilities& capabilities,
                   const mojom::FolderFeature& folder_feature,
                   const std::string& doc_id,
                   const std::string& alternate_url);

  void DisplayConfirmDialog(
      drivefs::mojom::DialogReasonPtr reason,
      drivefs::mojom::DriveFsDelegate::DisplayConfirmDialogCallback callback);

  mojo::Remote<drivefs::mojom::DriveFsDelegate>& delegate() {
    return delegate_;
  }

  MOCK_METHOD(void,
              GetSyncingPaths,
              (drivefs::mojom::DriveFs::GetSyncingPathsCallback callback),
              (override));

  MOCK_METHOD(void,
              StartSearchQuery,
              (mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
               drivefs::mojom::QueryParametersPtr query_params),
              (override));

  const base::FilePath& mount_path() { return mount_path_; }

  absl::optional<bool> IsItemPinned(const std::string& path);

  struct FileMetadata {
    FileMetadata();
    FileMetadata(const FileMetadata&);
    FileMetadata& operator=(const FileMetadata&);
    ~FileMetadata();

    std::string mime_type;
    bool pinned = false;
    bool hosted = false;
    bool shared = false;
    bool available_offline = false;
    std::string original_name;
    mojom::Capabilities capabilities;
    mojom::FolderFeature folder_feature;
    std::string doc_id;
    int64_t stable_id = 0;
    std::string alternate_url;
  };

  absl::optional<FakeDriveFs::FileMetadata> GetItemMetadata(
      const base::FilePath& path);

 private:
  class SearchQuery;

  // drivefs::mojom::DriveFsBootstrap:
  void Init(
      drivefs::mojom::DriveFsConfigurationPtr config,
      mojo::PendingReceiver<drivefs::mojom::DriveFs> receiver,
      mojo::PendingRemote<drivefs::mojom::DriveFsDelegate> delegate) override;

  // drivefs::mojom::DriveFs:
  void GetMetadata(const base::FilePath& path,
                   GetMetadataCallback callback) override;

  void GetMetadataByStableId(int64_t stable_id,
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

  void FetchAllChangeLogs() override;

  void FetchChangeLog(
      std::vector<mojom::FetchChangeLogOptionsPtr> options) override;

  void SendNativeMessageRequest(
      const std::string& request,
      SendNativeMessageRequestCallback callback) override;

  void SetStartupArguments(const std::string& arguments,
                           SetStartupArgumentsCallback callback) override;

  void GetStartupArguments(GetStartupArgumentsCallback callback) override;

  void SetTracingEnabled(bool enabled) override;

  void SetNetworkingEnabled(bool enabled) override;

  void ForcePauseSyncing(bool enable) override;

  void DumpAccountSettings() override;

  void LoadAccountSettings() override;

  void CreateNativeHostSession(
      drivefs::mojom::ExtensionConnectionParamsPtr params,
      mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> session,
      mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> port) override;

  void LocateFilesByItemIds(
      const std::vector<std::string>& item_ids,
      drivefs::mojom::DriveFs::LocateFilesByItemIdsCallback callback) override;

  void GetQuotaUsage(
      drivefs::mojom::DriveFs::GetQuotaUsageCallback callback) override;

  void GetPooledQuotaUsage(
      drivefs::mojom::DriveFs::GetPooledQuotaUsageCallback callback) override;

  void SetPinnedByStableId(int64_t stable_id,
                           bool pinned,
                           SetPinnedCallback callback) override;

  void ToggleMirroring(
      bool enabled,
      drivefs::mojom::DriveFs::ToggleMirroringCallback callback) override;

  void ToggleSyncForPath(
      const base::FilePath& path,
      drivefs::mojom::MirrorPathStatus status,
      drivefs::mojom::DriveFs::ToggleSyncForPathCallback callback) override;

  void PollHostedFilePinStates() override;

  void CancelUploadByPath(const base::FilePath& path) override;

  const base::FilePath mount_path_;
  int64_t next_stable_id_ = 1;

  std::map<base::FilePath, FileMetadata> metadata_;

  mojo::Receiver<drivefs::mojom::DriveFs> receiver_{this};
  mojo::Remote<drivefs::mojom::DriveFsDelegate> delegate_;
  mojo::Receiver<drivefs::mojom::DriveFsBootstrap> bootstrap_receiver_{this};
  mojo::PendingReceiver<drivefs::mojom::DriveFsDelegate>
      pending_delegate_receiver_;

  std::vector<base::FilePath> syncing_paths_;

  base::WeakPtrFactory<FakeDriveFs> weak_factory_{this};
};

}  // namespace drivefs

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_FAKE_DRIVEFS_H_
