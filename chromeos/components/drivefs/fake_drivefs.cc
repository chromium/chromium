// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/drivefs/fake_drivefs.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "chromeos/components/drivefs/drivefs_util.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace drivefs {
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
          base::FilePath(base::StringPiece(option).substr(strlen("datadir=")));
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
  NOTREACHED() << datadir_suffix;
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

struct FakeDriveFs::FileMetadata {
  std::string mime_type;
  bool pinned = false;
  bool hosted = false;
  bool shared = false;
  std::string original_name;
  mojom::Capabilities capabilities;
  mojom::FolderFeature folder_feature;
};

class FakeDriveFs::SearchQuery : public mojom::SearchQuery {
 public:
  SearchQuery(base::WeakPtr<FakeDriveFs> drive_fs,
              drivefs::mojom::QueryParametersPtr params)
      : drive_fs_(std::move(drive_fs)), params_(std::move(params)) {}

 private:
  void GetNextPage(GetNextPageCallback callback) override {
    if (!drive_fs_) {
      std::move(callback).Run(drive::FileError::FILE_ERROR_ABORT, {});
    } else {
      // Default implementation: just search for a file name.
      callback_ = std::move(callback);
      base::PostTaskAndReplyWithResult(
          FROM_HERE,
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT},
          base::BindOnce(&SearchQuery::SearchFiles, drive_fs_->mount_path()),
          base::BindOnce(&SearchQuery::GetMetadata,
                         weak_ptr_factory_.GetWeakPtr()));
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

      // Filter out non-matching results.
      base::EraseIf(results_, [=](const auto& item_ptr) {
        if (!item_ptr->metadata) {
          return true;
        }
        const base::FilePath path = item_ptr->path;
        const drivefs::mojom::FileMetadata* metadata = item_ptr->metadata.get();
        if (!query.empty()) {
          return base::ToLowerASCII(path.BaseName().value()).find(query) ==
                 std::string::npos;
        }
        if (params_->available_offline) {
          return !metadata->available_offline && IsLocal(metadata->type);
        }
        if (params_->shared_with_me) {
          return !metadata->shared;
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

  base::WeakPtrFactory<SearchQuery> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SearchQuery);
};

FakeDriveFs::FakeDriveFs(const base::FilePath& mount_path)
    : mount_path_(mount_path) {
  CHECK(mount_path.IsAbsolute());
  CHECK(!mount_path.ReferencesParent());
}

FakeDriveFs::~FakeDriveFs() = default;

void FakeDriveFs::RegisterMountingForAccountId(
    base::RepeatingCallback<std::string()> account_id_getter) {
  chromeos::DBusThreadManager* dbus_thread_manager =
      chromeos::DBusThreadManager::Get();
  static_cast<chromeos::FakeCrosDisksClient*>(
      dbus_thread_manager->GetCrosDisksClient())
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

void FakeDriveFs::SetMetadata(const base::FilePath& path,
                              const std::string& mime_type,
                              const std::string& original_name,
                              bool pinned,
                              bool shared,
                              const mojom::Capabilities& capabilities,
                              const mojom::FolderFeature& folder_feature) {
  auto& stored_metadata = metadata_[path];
  stored_metadata.mime_type = mime_type;
  stored_metadata.original_name = original_name;
  stored_metadata.hosted = (original_name != path.BaseName().value());
  stored_metadata.capabilities = capabilities;
  stored_metadata.folder_feature = folder_feature;
  if (pinned) {
    stored_metadata.pinned = true;
  }
  if (shared) {
    stored_metadata.shared = true;
  }
}

void FakeDriveFs::Init(
    drivefs::mojom::DriveFsConfigurationPtr config,
    mojo::PendingReceiver<drivefs::mojom::DriveFs> receiver,
    mojo::PendingRemote<drivefs::mojom::DriveFsDelegate> delegate) {
  {
    base::ScopedAllowBlockingForTesting allow_io;
    CHECK(base::CreateDirectory(mount_path_.Append(".Trash")));
  }
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

  const auto& stored_metadata = metadata_[path];
  metadata->pinned = stored_metadata.pinned;
  metadata->available_offline = stored_metadata.pinned;
  metadata->shared = stored_metadata.shared;

  metadata->content_mime_type = stored_metadata.mime_type;
  metadata->type = stored_metadata.hosted
                       ? mojom::FileMetadata::Type::kHosted
                       : info.is_directory
                             ? mojom::FileMetadata::Type::kDirectory
                             : mojom::FileMetadata::Type::kFile;

  base::StringPiece prefix;
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
  metadata->capabilities = stored_metadata.capabilities.Clone();

  std::move(callback).Run(drive::FILE_ERROR_OK, std::move(metadata));
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
  std::move(callback).Run(base::nullopt);
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
  std::move(callback).Run(drive::FILE_ERROR_OK);
}

void FakeDriveFs::StartSearchQuery(
    mojo::PendingReceiver<drivefs::mojom::SearchQuery> receiver,
    drivefs::mojom::QueryParametersPtr query_params) {
  auto search_query = std::make_unique<SearchQuery>(weak_factory_.GetWeakPtr(),
                                                    std::move(query_params));
  mojo::MakeSelfOwnedReceiver(std::move(search_query), std::move(receiver));
}

void FakeDriveFs::FetchAllChangeLogs() {}

void FakeDriveFs::FetchChangeLog(
    std::vector<mojom::FetchChangeLogOptionsPtr> options) {}

void FakeDriveFs::SendNativeMessageRequest(
    const std::string& request,
    SendNativeMessageRequestCallback callback) {
  std::move(callback).Run(drive::FILE_ERROR_SERVICE_UNAVAILABLE, "");
}

}  // namespace drivefs
