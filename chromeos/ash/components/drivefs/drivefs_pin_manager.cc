// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/drivefs/drivefs_pin_manager.h"

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/drive/file_errors.h"

namespace drivefs::pinning {

namespace {

mojom::QueryParametersPtr CreateMyDriveQuery(int32_t page_size) {
  mojom::QueryParametersPtr query = mojom::QueryParameters::New();
  query->page_size = page_size;
  query->query_kind = mojom::QueryKind::kRegular;
  query->query_source = mojom::QueryParameters::QuerySource::kCloudOnly;
  // TODO(b/259454320): The query.proto for this says the C++ clients don't
  // handle `false` for this boolean, need to investigate if that is true or
  // not.
  query->available_offline = false;
  query->shared_with_me = false;
  return query;
}

class FreeDiskSpaceImpl : public FreeDiskSpaceDelegate {
 public:
  FreeDiskSpaceImpl() = default;

  FreeDiskSpaceImpl(const FreeDiskSpaceImpl&) = delete;
  FreeDiskSpaceImpl& operator=(const FreeDiskSpaceImpl&) = delete;

  ~FreeDiskSpaceImpl() override = default;

  void AmountOfFreeDiskSpace(
      const base::FilePath& path,
      base::OnceCallback<void(int64_t)> callback) override {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
        std::move(callback));
  }
};

}  // namespace

constexpr char kGCacheFolderName[] = "GCache";

DriveFsPinManager::DriveFsPinManager(bool enabled,
                                     const base::FilePath& profile_path,
                                     mojom::DriveFs* drivefs_interface)
    : enabled_(enabled),
      free_disk_space_(std::make_unique<FreeDiskSpaceImpl>()),
      profile_path_(profile_path),  // The GCache directory is located in the
                                    // users profile path.
      drivefs_interface_(drivefs_interface) {}

DriveFsPinManager::DriveFsPinManager(
    bool enabled,
    const base::FilePath& profile_path,
    mojom::DriveFs* drivefs_interface,
    std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space)
    : DriveFsPinManager(enabled, profile_path, drivefs_interface) {
  free_disk_space_ = std::move(free_disk_space);
}

DriveFsPinManager::~DriveFsPinManager() = default;

// TODO(b/259454320): Pass through a `base::RepeatingCallback` here to enable
// the callsite to receive progress updates.
void DriveFsPinManager::Start(
    base::OnceCallback<void(PinError)> complete_callback) {
  if (!enabled_) {
    LOG(ERROR) << "The pin manager is not enabled";
    std::move(complete_callback).Run(PinError::kManagerDisabled);
    return;
  }

  VLOG(1) << "Starting to search for items to calculate required space";
  timer_.Begin();
  complete_callback_ = std::move(complete_callback);

  base::FilePath gcache_path(profile_path_.AppendASCII(kGCacheFolderName));

  free_disk_space_->AmountOfFreeDiskSpace(
      gcache_path, base::BindOnce(&DriveFsPinManager::OnFreeDiskSpaceRetrieved,
                                  weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnFreeDiskSpaceRetrieved(int64_t free_space) {
  if (free_space == -1) {
    LOG(ERROR) << "Error calculating free disk space";
    std::move(complete_callback_).Run(PinError::kErrorCalculatingFreeDiskSpace);
    return;
  }

  free_space_ = free_space;

  VLOG(1) << "Starting to search for items to calculate required space";
  // TODO(b/259454320): 50 is chosen arbitrarily, this needs to be updated as
  // different batch sizes are experimented with.
  mojom::QueryParametersPtr query = CreateMyDriveQuery(50);
  drivefs_interface_->StartSearchQuery(
      search_query_.BindNewPipeAndPassReceiver(), std::move(query));
  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsPinManager::OnSearchResultForSizeCalculation(
    drive::FileError error,
    absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items) {
  if (error != drive::FILE_ERROR_OK) {
    LOG(ERROR) << "Error retrieving search results for size calculation: "
               << error;
    std::move(complete_callback_).Run(PinError::kErrorRetrievingSearchResults);
    return;
  }

  if (!items.has_value()) {
    LOG(ERROR) << "Items returned are invalid";
    std::move(complete_callback_).Run(PinError::kErrorResultsReturnedInvalid);
    return;
  }

  if (items.value().size() == 0) {
    VLOG(1) << "Iterated all files and calculated " << size_required_
            << " bytes required with " << free_space_ << " bytes available in "
            << timer_.Elapsed().InMilliseconds() << "ms";
    std::move(complete_callback_).Run(PinError::kSuccess);
    return;
  }

  for (const auto& item : items.value()) {
    if (item->metadata->pinned) {
      VLOG(1) << "Item is already pinned, ignoring in space calculation";
      continue;
    }
    size_required_ += item->metadata->size;
  }

  // TODO(b/259454320): This should really not use up all free space but instead
  // include a buffer threshold. Update this once the thresholds have been
  // identified.
  if (size_required_ >= free_space_) {
    LOG(ERROR) << "The required size (" << size_required_
               << " bytes) exceeds the available free space (" << free_space_
               << "bytes)";
    std::move(complete_callback_).Run(PinError::kErrorNotEnoughFreeSpace);
    return;
  }

  search_query_->GetNextPage(
      base::BindOnce(&DriveFsPinManager::OnSearchResultForSizeCalculation,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace drivefs::pinning
