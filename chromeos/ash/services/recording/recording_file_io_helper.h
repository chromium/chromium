// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_FILE_IO_HELPER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_FILE_IO_HELPER_H_

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/services/recording/public/mojom/recording_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace recording {

namespace mojom {
enum class RecordingStatus;
}  // namespace mojom

// Defines a helper that can be used to perform remaining free space or
// remaining DriveFs quota checks when writing bytes to the output file.
class RecordingFileIoHelper {
 public:
  // Defines an API so that the helper can notify with any IO errors that happen
  // while writing to the file, or when the remaining disk space / DriveFS quota
  // are less than the minimum threshold.
  class Delegate {
   public:
    virtual void NotifyFailure(mojom::RecordingStatus status) = 0;

   protected:
    virtual ~Delegate() = default;
  };

  RecordingFileIoHelper(
      const base::FilePath& output_file_path,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      Delegate* delegate);
  RecordingFileIoHelper(const RecordingFileIoHelper&) = delete;
  RecordingFileIoHelper& operator=(const RecordingFileIoHelper&) = delete;
  ~RecordingFileIoHelper();

  Delegate* delegate() { return delegate_; }

  // Tells this helper that `num_bytes` have been written to the output file so
  // that it can perform any remaining disk space checks if needed.
  void OnBytesWritten(int64_t num_bytes);

 private:
  // Returns true if the video file is being written to a path
  // `output_file_path_` that exists in DriveFS, false if it's a local file.
  bool IsDriveFsFile() const;

  // Checks the remaining free space (whether for a local file, or a DriveFS
  // file) once `num_bytes_till_next_disk_space_check_` goes below zero.
  void MaybeCheckRemainingSpace();

  // Called to test the `remaining_free_space_bytes` against the minimum
  // threshold below which we end the recording with a failure. The failure type
  // that will be propagated to the client is the given `status`.
  void OnGotRemainingFreeSpace(mojom::RecordingStatus status,
                               int64_t remaining_free_space_bytes);

  // The path of the output file to which the encoder output will be written.
  const base::FilePath output_file_path_;

  // A remote end to the DriveFS delegate that can calculate the remaining free
  // space in Drive. This is bound only when the `output_file_path_` points to a
  // file in DriveFS. Being unbound means the file is a local disk file.
  mojo::Remote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate_remote_;

  // The `RecordingEncoder` that owns this helper (either directly or
  // indirectly) which acts as a delegate of this class to notify with any IO
  // errors.
  const raw_ptr<Delegate> delegate_;

  // Once this value becomes <= 0, we trigger a remaining disk space poll.
  // Initialized to 0, so that we poll the disk space on the very first write
  // operation.
  int64_t num_bytes_till_next_disk_space_check_ = 0;

  // True when we're waiting for a reply from the remote DriveFS quota delegate.
  bool waiting_for_drive_fs_delegate_ = false;

  base::WeakPtrFactory<RecordingFileIoHelper> weak_ptr_factory_{this};
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_RECORDING_FILE_IO_HELPER_H_
