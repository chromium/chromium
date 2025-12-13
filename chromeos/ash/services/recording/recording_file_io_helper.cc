// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/recording_file_io_helper.h"

#include "base/byte_count.h"
#include "base/strings/string_util.h"
#include "base/system/sys_info.h"

namespace recording {

namespace {

// We use a threshold of 512 MB to end the video recording due to low disk
// space, which is the same threshold as that used by the low disk space
// notification (See low_disk_notification.cc).
constexpr base::ByteCount kLowDiskSpaceThreshold = base::MiB(512);

// To avoid checking the remaining disk space after every write operation, we do
// it only once every 10 MB written of encoder data.
constexpr base::ByteCount kMinDataBetweenDiskSpaceChecks = base::MiB(10);

}  // namespace

RecordingFileIoHelper::RecordingFileIoHelper(
    const base::FilePath& output_file_path,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    Delegate* delegate)
    : output_file_path_(output_file_path),
      drive_fs_quota_delegate_remote_(std::move(drive_fs_quota_delegate)),
      delegate_(delegate) {
  DCHECK(delegate_);
}

RecordingFileIoHelper::~RecordingFileIoHelper() = default;

void RecordingFileIoHelper::OnBytesWritten(int64_t num_bytes) {
  data_till_next_disk_space_check_ -= base::ByteCount(num_bytes);
  MaybeCheckRemainingSpace();
}

bool RecordingFileIoHelper::IsDriveFsFile() const {
  return drive_fs_quota_delegate_remote_.is_bound();
}

void RecordingFileIoHelper::MaybeCheckRemainingSpace() {
  if (data_till_next_disk_space_check_ > base::ByteCount(0)) {
    return;
  }

  if (!IsDriveFsFile()) {
    OnGotRemainingFreeSpace(
        mojom::RecordingStatus::kLowDiskSpace,
        base::SysInfo::AmountOfFreeDiskSpace(output_file_path_).value_or(-1));
    return;
  }

  if (waiting_for_drive_fs_delegate_) {
    return;
  }

  DCHECK(drive_fs_quota_delegate_remote_);
  waiting_for_drive_fs_delegate_ = true;
  drive_fs_quota_delegate_remote_->GetDriveFsFreeSpaceBytes(
      base::BindOnce(&RecordingFileIoHelper::OnGotRemainingFreeSpace,
                     weak_ptr_factory_.GetWeakPtr(),
                     mojom::RecordingStatus::kLowDriveFsQuota));
}

void RecordingFileIoHelper::OnGotRemainingFreeSpace(
    mojom::RecordingStatus status,
    int64_t remaining_free_space_bytes) {
  waiting_for_drive_fs_delegate_ = false;
  data_till_next_disk_space_check_ = kMinDataBetweenDiskSpaceChecks;

  if (remaining_free_space_bytes < 0) {
    // A negative value (e.g. -1) indicates a failure in computing the free
    // space.
    return;
  }

  if (remaining_free_space_bytes < kLowDiskSpaceThreshold.InBytes()) {
    LOG(WARNING) << "Ending recording due to " << status
                 << ", and remaining free space of "
                 << base::ByteCount(remaining_free_space_bytes);
    delegate_->NotifyFailure(status);
  }
}

}  // namespace recording
