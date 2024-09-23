// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/fake_drivefs.h"

#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/ash/components/dbus/cros_disks/cros_disks_client.h"
#include "chromeos/ash/components/dbus/cros_disks/fake_cros_disks_client.h"
#include "chromeos/ash/components/drivefs/drivefs_util.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-shared.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/mime_util.h"
#include "url/gurl.h"

namespace drivefs {

FakeMetadata::FakeMetadata() = default;
FakeMetadata::~FakeMetadata() = default;

FakeMetadata::FakeMetadata(FakeMetadata&& other) = default;
FakeMetadata& FakeMetadata::operator=(FakeMetadata&& other) = default;

namespace {

std::vector<std::pair<base::RepeatingCallback<std::string()>,
                      base::WeakPtr<FakeDriveFs>>>&
GetRegisteredFakeDriveFsIntances() {
  static base::NoDestructor<std::vector<std::pair<
      base::RepeatingCallback<std::string()>, base::WeakPtr<FakeDriveFs>>>>
      registered_fake_drivefs_instances;
  return *registered_fake_drivefs_instances;
}

base::FilePath MaybeMountDriveFs(
    const std::string& source_path,
    const std::vector<std::string>& mount_options) {
  GURL source_url(source_path);
  DCHECK(source_url.is_valid());
  if (source_url.scheme() != "drivefs") {
    return {};
  }
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
  CHECK(!datadir_suffix.empty());
  for (auto& registration : GetRegisteredFakeDriveFsIntances()) {
    std::string account_id = registration.first.Run();
    if (registration.second && !account_id.empty() &&
        account_id == datadir_suffix) {
      return registration.second->mount_path();
    }
  }
  NOTREACHED_IN_MIGRATION() << datadir_suffix;
  return {};
}

}  // namespace

FakeDriveFsBootstrapListener::FakeDriveFsBootstrapListener(
    mojo::PendingRemote<drivefs::mojom::DriveFsBootstrap> bootstrap)
    : bootstrap_(std::move(bootstrap)) {}

FakeDriveFsBootstrapListener::~FakeDriveFsBootstrapListener() = default;

void FakeDriveFsBootstrapListener::SendInvitationOverPipe(base::ScopedFD) {}

mojo::PendingRemote<mojom::DriveFsBootstrap>
FakeDriveFsBootstrapListener::bootstrap() {
  return std::move(bootstrap_);
}

class FakeDriveFs::SearchQuery : public mojom::SearchQuery {
 public:
  SearchQuery(base::WeakPtr<FakeDriveFs> drive_fs,
              drivefs::mojom::QueryParametersPtr params)
      : drive_fs_(std::move(drive_fs)), params_(std::move(params)) {}

  SearchQuery(const SearchQuery&) = delete;
  SearchQuery& operator=(const SearchQuery&) = delete;

 private:
  void GetNextPage(GetNextPageCallback callback) override {
    if (!drive_fs_) {
      std::move(callback).Run(drive::FileError::FILE_ERROR_ABORT, {});
    } else if (next_page_called_) {
      // If GetNextPage was previously called, on the next request send an empty
      // array. This is the current way to identify if a query has no more
      // results, however, this is slightly incorrect and b/277018122 tracks
      // providing a more robust fix.
      next_page_called_ = false;
      std::move(callback).Run(drive::FILE_ERROR_OK, {});
    } else {
      // Default implementation: just search for a file name.
      callback_ = std::move(callback);
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&SearchQuery::SearchFiles, drive_fs_->mount_path()),
          base::BindOnce(&SearchQuery::GetMetadata,
                         weak_ptr_factory_.GetWeakPtr()));
      next_page_called_ = true;
    }
  }

  static std::vector<drivefs::mojom::QueryItemPtr> SearchFiles(
      const base::FilePath& mount_path) {
    std::vector<drivefs::mojom::QueryItemPtr> results;
    base::FileEnumerator walker(
        mount_path, true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    for (auto file = walker.Next(); !file.empty(); file = walker.Next()) {
      auto item = drivefs::mojom::QueryItem::New();
      item->path = base::FilePath("/");
      CHECK(mount_path.AppendRelativePath(file, &item->path));
      results.push_back(std::move(item));
    }
    return results;
  }

  void GetMetadata(std::vector<drivefs::mojom::QueryItemPtr> results) {
    if (!drive_fs_) {
      std::move(callback_).Run(drive::FileError::FILE_ERROR_ABORT, {});
    } else {
      results_ = std::move(results);
      pending_callbacks_ = results_.size() + 1;
      for (size_t i = 0; i < results_.size(); ++i) {
        drive_fs_->GetMetadata(
            results_[i]->path,
            base::BindOnce(&SearchQuery::OnMetadata,
                           weak_ptr_factory_.GetWeakPtr(), i));
      }
      OnComplete();
    }
  }

  void OnMetadata(size_t index,
                  drive::FileError error,
                  drivefs::mojom::FileMetadataPtr metadata) {
    if (error == drive::FileError::FILE_ERROR_OK) {
      results_[index]->metadata = std::move(metadata);
    }
    OnComplete();
  }

  void OnComplete() {
    if (--pending_callbacks_ == 0) {
      auto query = base::ToLowerASCII(
          params_->title.value_or(params_->text_content.value_or("")));
      std::vector<std::string> mime_types;
      if (params_->mime_types.has_value()) {
        mime_types = params_->mime_types.value();
      }
      if (params_->mime_type.has_value()) {
        mime_types.emplace_back(params_->mime_type.value() + "/*");
      }

      // Filter out non-matching results.
      std::erase_if(results_, [=, this](const auto& item_ptr) {
        if (!item_ptr->metadata) {
          return true;
        }
        const base::FilePath path = item_ptr->path;
        const drivefs::mojom::FileMetadata* metadata = item_ptr->metadata.get();
        if (!query.empty()) {
          if (!base::Contains(base::ToLowerASCII(path.BaseName().value()),
                              query)) {
            return true;
          }
        }
        if (params_->available_offline) {
          if (!metadata->available_offline && IsLocal(metadata->type)) {
            return true;
          }
        }
        if (params_->shared_with_me) {
          if (!metadata->shared) {
            return true;
          }
        }
        if (params_->modified_time.has_value()) {
          switch (params_->modified_time_operator) {
            case mojom::QueryParameters::DateComparisonOperator::kLessThan:
              if (metadata->modification_time >= *params_->modified_time) {
                return true;
              }
              break;
            case mojom::QueryParameters::DateComparisonOperator::kLessOrEqual:
              if (metadata->modification_time > *params_->modified_time) {
                return true;
              }
              break;
            case mojom::QueryParameters::DateComparisonOperator::kEqual:
              if (metadata->modification_time != *params_->modified_time) {
                return true;
              }
              break;
            case mojom::QueryParameters::DateComparisonOperator::
                kGreaterOrEqual:
              if (metadata->modification_time < *params_->modified_time) {
                return true;
              }
              break;
            case mojom::QueryParameters::DateComparisonOperator::kGreaterThan:
              if (metadata->modification_time <= *params_->modified_time) {
                return true;
              }
              break;
            default:
              break;
          }
        }
        if (!mime_types.empty()) {
          std::string content_mime_type = metadata->content_mime_type;
          // If we do not know the MIME type the file may or may not match. Thus
          // we only test MIME type match if we know the files MIME type.
          if (!content_mime_type.empty()) {
            if (base::ranges::none_of(
                    mime_types,
                    [content_mime_type](const std::string& mime_type) {
                      return net::MatchesMimeType(mime_type, content_mime_type);
                    })) {
              return true;
            }
          }
        }
        return false;
      });

      const auto sort_direction = params_->sort_direction;
      switch (params_->sort_field) {
        case mojom::QueryParameters::SortField::kLastModified:
        case mojom::QueryParameters::SortField::kLastViewedByMe:
          std::sort(
              results_.begin(), results_.end(),
              [sort_direction](const auto& a, const auto& b) {
                auto a_fields = std::tie(a->metadata->last_viewed_by_me_time,
                                         a->metadata->modification_time);
                auto b_fields = std::tie(b->metadata->last_viewed_by_me_time,
                                         b->metadata->modification_time);
                if (sort_direction ==
                    mojom::QueryParameters::SortDirection::kAscending) {
                  return a_fields < b_fields;
                }
                return b_fields < a_fields;
              });
          break;

        case mojom::QueryParameters::SortField::kSharedWithMe:
          NOTIMPLEMENTED();
          break;
        case mojom::QueryParameters::SortField::kFileSize:
          NOTIMPLEMENTED();
          break;

        case mojom::QueryParameters::SortField::kNone:
          break;
      }

      auto page_size = base::saturated_cast<size_t>(params_->page_size);
      if (results_.size() > page_size) {
        results_.resize(page_size);
      }
      std::move(callback_).Run(drive::FileError::FILE_ERROR_OK,
                               {std::move(results_)});
    }
  }

  base::WeakPtr<FakeDriveFs> drive_fs_;
  mojom::QueryParametersPtr params_;
  GetNextPageCallback callback_;
  std::vector<drivefs::mojom::QueryItemPtr> results_;
  size_t pending_callbacks_ = 0;

  bool next_page_called_ = false;

  base::WeakPtrFactory<SearchQuery> weak_ptr_factory_{this};
};

FakeDriveFs::FileMetadata::FileMetadata() = default;
FakeDriveFs::FileMetadata::FileMetadata(const FakeDriveFs::FileMetadata&) =
    default;
FakeDriveFs::FileMetadata& FakeDriveFs::FileMetadata::operator=(
    const FakeDriveFs::FileMetadata&) = default;
FakeDriveFs::FileMetadata::~FileMetadata() = default;

FakeDriveFs::FakeDriveFs(const base::FilePath& mount_path)
    : mount_path_(mount_path) {
  CHECK(mount_path.IsAbsolute());
  CHECK(!mount_path.ReferencesParent());

  ON_CALL(*this, StartSearchQuery)
      .WillByDefault(
          [this](mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
                 drivefs::mojom::QueryParametersPtr query_params) {
            auto search_query = std::make_unique<SearchQuery>(
                weak_factory_.GetWeakPtr(), std::move(query_params));
            mojo::MakeSelfOwnedReceiver(std::move(search_query),
                                        std::move(receiver));
          });
}

FakeDriveFs::~FakeDriveFs() = default;

void FakeDriveFs::RegisterMountingForAccountId(
    base::RepeatingCallback<std::string()> account_id_getter) {
  static_cast<ash::FakeCrosDisksClient*>(ash::CrosDisksClient::Get())
      ->AddCustomMountPointCallback(base::BindRepeating(&MaybeMountDriveFs));

  GetRegisteredFakeDriveFsIntances().emplace_back(std::move(account_id_getter),
                                                  weak_factory_.GetWeakPtr());
}

std::unique_ptr<drivefs::DriveFsBootstrapListener>
FakeDriveFs::CreateMojoListener() {
  delegate_.reset();
  pending_delegate_receiver_ = delegate_.BindNewPipeAndPassReceiver();
  delegate_->OnMounted();

  bootstrap_receiver_.reset();
  return std::make_unique<FakeDriveFsBootstrapListener>(
      bootstrap_receiver_.BindNewPipeAndPassRemote());
}

void FakeDriveFs::SetMetadata(const FakeMetadata& metadata) {
  auto& stored_metadata = metadata_[metadata.path];
  stored_metadata.mime_type = metadata.mime_type;
  stored_metadata.original_name = metadata.original_name;
  stored_metadata.hosted =
      (metadata.original_name != metadata.path.BaseName().value());
  stored_metadata.capabilities = metadata.capabilities;
  stored_metadata.folder_feature = metadata.folder_feature;
  stored_metadata.doc_id = metadata.doc_id;
  stored_metadata.pinned = metadata.pinned;
  stored_metadata.dirty = metadata.dirty;
  stored_metadata.available_offline = metadata.available_offline;
  stored_metadata.shared = metadata.shared;
  if (metadata.shortcut) {
    drivefs::mojom::ShortcutDetails shortcut_details;
    shortcut_details.target_lookup_status =
        drivefs::mojom::ShortcutDetails::LookupStatus::kOk;
    if (!metadata.shortcut_target_path.empty()) {
      shortcut_details.target_path =
          std::make_optional<base::FilePath>(metadata.shortcut_target_path);
    }
    stored_metadata.shortcut_details = std::move(shortcut_details);
  }
  stored_metadata.alternate_url = metadata.alternate_url;
  stored_metadata.can_pin = metadata.can_pin;
}

void FakeDriveFs::DisplayConfirmDialog(
    drivefs::mojom::DialogReasonPtr reason,
    drivefs::mojom::DriveFsDelegate::DisplayConfirmDialogCallback callback) {
  DCHECK(delegate_);
  delegate_->DisplayConfirmDialog(std::move(reason), std::move(callback));
}

std::optional<bool> FakeDriveFs::IsItemPinned(const std::string& path) {
  for (const auto& metadata : metadata_) {
    if (metadata.first.value() == path) {
      return metadata.second.pinned;
    }
  }
  return std::nullopt;
}

std::optional<bool> FakeDriveFs::IsItemDirty(const std::string& path) {
  for (const auto& metadata : metadata_) {
    if (metadata.first.value() == path) {
      return metadata.second.dirty;
    }
  }
  return std::nullopt;
}

bool FakeDriveFs::SetCanPin(const std::string& path, bool can_pin) {
  for (auto& metadata : metadata_) {
    if (metadata.first.value() == path) {
      metadata.second.can_pin = can_pin;
      return true;
    }
  }
  return false;
}

std::optional<FakeDriveFs::FileMetadata> FakeDriveFs::GetItemMetadata(
    const base::FilePath& path) {
  const auto& metadata = metadata_.find(path);
  if (metadata == metadata_.end()) {
    return std::nullopt;
  }
  if (metadata->second.stable_id == 0) {
    metadata->second.stable_id = next_stable_id_++;
  }
  return metadata->second;
}

void FakeDriveFs::Init(
    drivefs::mojom::DriveFsConfigurationPtr config,
    mojo::PendingReceiver<drivefs::mojom::DriveFs> receiver,
    mojo::PendingRemote<drivefs::mojom::DriveFsDelegate> delegate) {
  mojo::FusePipes(std::move(pending_delegate_receiver_), std::move(delegate));
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void FakeDriveFs::GetMetadata(const base::FilePath& path,
                              GetMetadataCallback callback) {
  base::FilePath absolute_path = mount_path_;
  CHECK(base::FilePath("/").AppendRelativePath(path, &absolute_path));
  base::File::Info info;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    if (!base::GetFileInfo(absolute_path, &info)) {
      std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
      return;
    }
  }
  auto metadata = drivefs::mojom::FileMetadata::New();
  metadata->size = info.size;
  metadata->modification_time = info.last_modified;
  metadata->last_viewed_by_me_time = info.last_accessed;

  if (metadata_[path].stable_id == 0) {
    metadata_[path].stable_id = next_stable_id_++;
  }

  const auto& stored_metadata = metadata_[path];
  metadata->item_id = stored_metadata.doc_id;
  metadata->pinned = stored_metadata.pinned;
  metadata->dirty = stored_metadata.dirty;
  metadata->available_offline =
      stored_metadata.pinned || stored_metadata.available_offline;
  metadata->shared = stored_metadata.shared;

  metadata->content_mime_type = stored_metadata.mime_type;
  metadata->type = stored_metadata.hosted ? mojom::FileMetadata::Type::kHosted
                   : info.is_directory ? mojom::FileMetadata::Type::kDirectory
                                       : mojom::FileMetadata::Type::kFile;

  if (!stored_metadata.alternate_url.empty()) {
    metadata->alternate_url = stored_metadata.alternate_url;
  } else {
    std::string_view prefix;
    if (stored_metadata.hosted) {
      prefix = "https://document_alternate_link/";
    } else if (info.is_directory) {
      prefix = "https://folder_alternate_link/";
    } else {
      prefix = "https://file_alternate_link/";
    }
    std::string suffix = stored_metadata.original_name.empty()
                             ? path.BaseName().value()
                             : stored_metadata.original_name;
    metadata->alternate_url = GURL(base::StrCat({prefix, suffix})).spec();
  }

  metadata->capabilities = stored_metadata.capabilities.Clone();
  metadata->stable_id = stored_metadata.stable_id;
  using CanPinStatus = mojom::FileMetadata::CanPinStatus;
  metadata->can_pin =
      (stored_metadata.can_pin) ? CanPinStatus::kOk : CanPinStatus::kDisabled;
  if (stored_metadata.hosted) {
    metadata->can_pin = CanPinStatus::kDisabled;
  }
  if (stored_metadata.shortcut_details.has_value()) {
    metadata->shortcut_details = stored_metadata.shortcut_details->Clone();
  }

  std::move(callback).Run(drive::FILE_ERROR_OK, std::move(metadata));
}

void FakeDriveFs::GetMetadataByStableId(int64_t stable_id,
                                        GetMetadataCallback callback) {
  for (const auto& [path, metadata] : metadata_) {
    if (metadata.stable_id == stable_id) {
      GetMetadata(path, std::move(callback));
      return;
    }
  }
  std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND, nullptr);
}

void FakeDriveFs::SetPinned(const base::FilePath& path,
                            bool pinned,
                            SetPinnedCallback callback) {
  metadata_[path].pinned = pinned;
  std::move(callback).Run(drive::FILE_ERROR_OK);
}

void FakeDriveFs::UpdateNetworkState(bool pause_syncing, bool is_offline) {}

void FakeDriveFs::ResetCache(ResetCacheCallback callback) {
  std::move(callback).Run(drive::FILE_ERROR_OK);
}

void FakeDriveFs::GetThumbnail(const base::FilePath& path,
                               bool crop_to_square,
                               GetThumbnailCallback callback) {
  std::move(callback).Run(std::nullopt);
}

void FakeDriveFs::CopyFile(const base::FilePath& source,
                           const base::FilePath& target,
                           CopyFileCallback callback) {
  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath source_absolute_path = mount_path_;
  base::FilePath target_absolute_path = mount_path_;
  CHECK(base::FilePath("/").AppendRelativePath(source, &source_absolute_path));
  CHECK(base::FilePath("/").AppendRelativePath(target, &target_absolute_path));

  base::File::Info source_info;
  if (!base::GetFileInfo(source_absolute_path, &source_info)) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (source_info.is_directory) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_A_FILE);
    return;
  }

  base::File::Info target_directory_info;
  if (!base::GetFileInfo(target_absolute_path.DirName(),
                         &target_directory_info)) {
    std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND);
    return;
  }
  if (!target_directory_info.is_directory) {
    std::move(callback).Run(drive::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (base::PathExists(target_absolute_path)) {
    std::move(callback).Run(drive::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (!base::CopyFile(source_absolute_path, target_absolute_path)) {
    std::move(callback).Run(drive::FILE_ERROR_FAILED);
    return;
  }
  metadata_[target_absolute_path] = metadata_[source_absolute_path];
  metadata_[target_absolute_path].stable_id = next_stable_id_++;
  std::move(callback).Run(drive::FILE_ERROR_OK);
}

void FakeDriveFs::FetchAllChangeLogs() {}

void FakeDriveFs::FetchChangeLog(
    std::vector<mojom::FetchChangeLogOptionsPtr> options) {}

void FakeDriveFs::SendNativeMessageRequest(
    const std::string& request,
    SendNativeMessageRequestCallback callback) {
  std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, "");
}

void FakeDriveFs::SetStartupArguments(const std::string& arguments,
                                      SetStartupArgumentsCallback callback) {
  std::move(callback).Run(false);
}

void FakeDriveFs::GetStartupArguments(GetStartupArgumentsCallback callback) {
  std::move(callback).Run("");
}

void FakeDriveFs::SetTracingEnabled(bool enabled) {}

void FakeDriveFs::SetNetworkingEnabled(bool enabled) {}

void FakeDriveFs::ForcePauseSyncing(bool enable) {}

void FakeDriveFs::DumpAccountSettings() {}

void FakeDriveFs::LoadAccountSettings() {}

void FakeDriveFs::CreateNativeHostSession(
    drivefs::mojom::ExtensionConnectionParamsPtr params,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> session,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> port) {}

void FakeDriveFs::LocateFilesByItemIds(
    const std::vector<std::string>& item_ids,
    drivefs::mojom::DriveFs::LocateFilesByItemIdsCallback callback) {
  base::flat_map<std::string, base::FilePath> results;
  {
    base::ScopedAllowBlockingForTesting allow_io;
    base::FileEnumerator enumerator(
        mount_path_, true,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    base::FilePath path = enumerator.Next();
    while (!path.empty()) {
      base::FilePath relative_path;
      CHECK(mount_path_.AppendRelativePath(path, &relative_path));
      const auto& stored_metadata =
          metadata_[base::FilePath("/").Append(relative_path)];
      if (!stored_metadata.doc_id.empty() &&
          base::Contains(item_ids, stored_metadata.doc_id)) {
        results[stored_metadata.doc_id] = relative_path;
      }
      path = enumerator.Next();
    }
  }
  std::vector<drivefs::mojom::FilePathOrErrorPtr> response;
  for (const auto& id : item_ids) {
    auto it = results.find(id);
    if (it == results.end()) {
      response.push_back(drivefs::mojom::FilePathOrError::NewError(
          drive::FileError::FILE_ERROR_NOT_FOUND));
    } else {
      response.push_back(drivefs::mojom::FilePathOrError::NewPath(it->second));
    }
  }
  std::move(callback).Run(std::move(response));
}

void FakeDriveFs::GetQuotaUsage(
    drivefs::mojom::DriveFs::GetQuotaUsageCallback callback) {
  std::move(callback).Run(drive::FileError::FILE_ERROR_SERVICE_UNAVAILABLE,
                          mojom::QuotaUsage::New());
}

void FakeDriveFs::SetPooledStorageQuotaUsage(int64_t used_user_bytes,
                                             int64_t total_user_bytes,
                                             bool organization_limit_exceeded) {
  pooled_quota_usage_.used_user_bytes = used_user_bytes;
  pooled_quota_usage_.total_user_bytes = total_user_bytes;
  pooled_quota_usage_.organization_limit_exceeded = organization_limit_exceeded;
}

void FakeDriveFs::GetPooledQuotaUsage(
    drivefs::mojom::DriveFs::GetPooledQuotaUsageCallback callback) {
  auto usage = mojom::PooledQuotaUsage::New();

  usage->user_type = pooled_quota_usage_.user_type;
  usage->used_user_bytes = pooled_quota_usage_.used_user_bytes;
  usage->total_user_bytes = pooled_quota_usage_.total_user_bytes;
  usage->organization_limit_exceeded =
      pooled_quota_usage_.organization_limit_exceeded;
  usage->organization_name = "Test organization";

  std::move(callback).Run(drive::FileError::FILE_ERROR_OK, std::move(usage));
}

void FakeDriveFs::SetPinnedByStableId(int64_t stable_id,
                                      bool pinned,
                                      SetPinnedCallback callback) {
  for (auto& [key, metadata] : metadata_) {
    if (metadata.stable_id == stable_id) {
      metadata.pinned = pinned;
      std::move(callback).Run(drive::FILE_ERROR_OK);
      return;
    }
  }
  std::move(callback).Run(drive::FILE_ERROR_NOT_FOUND);
}

void FakeDriveFs::ToggleMirroring(
    bool enabled,
    drivefs::mojom::DriveFs::ToggleMirroringCallback callback) {
  std::move(callback).Run(drivefs::mojom::MirrorSyncStatus::kSuccess);
}

void FakeDriveFs::ToggleSyncForPath(
    const base::FilePath& path,
    drivefs::mojom::MirrorPathStatus status,
    drivefs::mojom::DriveFs::ToggleSyncForPathCallback callback) {
  if (status == drivefs::mojom::MirrorPathStatus::kStart) {
    syncing_paths_.push_back(path);
  } else {
    // status == drivefs::mojom::MirrorPathStatus::kStop.
    auto element = base::ranges::find(syncing_paths_, path);
    syncing_paths_.erase(element);
  }
  std::move(callback).Run(drive::FileError::FILE_ERROR_OK);
}

void FakeDriveFs::PollHostedFilePinStates() {}

void FakeDriveFs::CancelUploadByPath(
    const base::FilePath& path,
    drivefs::mojom::DriveFs::CancelUploadMode cancel_mode) {}

void FakeDriveFs::SetDocsOfflineEnabled(
    bool enabled,
    drivefs::mojom::DriveFs::SetDocsOfflineEnabledCallback callback) {
  std::move(callback).Run(drive::FILE_ERROR_OK,
                          drivefs::mojom::DocsOfflineEnableStatus::kSuccess);
}

void FakeDriveFs::GetDocsOfflineStats(
    drivefs::mojom::DriveFs::GetDocsOfflineStatsCallback callback) {
  drivefs::mojom::DocsOfflineStatsPtr stats =
      drivefs::mojom::DocsOfflineStats::New();
  std::move(callback).Run(drive::FILE_ERROR_OK, std::move(stats));
}

void FakeDriveFs::GetMirrorSyncStatusForFile(
    const base::FilePath& path,
    drivefs::mojom::DriveFs::GetMirrorSyncStatusForFileCallback callback) {
  std::move(callback).Run(drivefs::mojom::MirrorItemSyncingStatus::kSynced);
}

void FakeDriveFs::GetMirrorSyncStatusForDirectory(
    const base::FilePath& path,
    drivefs::mojom::DriveFs::GetMirrorSyncStatusForDirectoryCallback callback) {
  std::move(callback).Run(drivefs::mojom::MirrorItemSyncingStatus::kSynced);
}

}  // namespace drivefs
