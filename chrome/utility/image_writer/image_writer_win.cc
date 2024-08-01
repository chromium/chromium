// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/utility/image_writer/image_writer.h"

#include <windows.h>

#include <setupapi.h>
#include <stddef.h>
#include <winioctl.h>

#include "base/containers/heap_array.h"
#include "base/logging.h"
#include "chrome/utility/image_writer/error_message_strings.h"

namespace image_writer {

const size_t kStorageQueryBufferSize = 1024;

bool ImageWriter::IsValidDevice() {
  base::win::ScopedHandle device_handle(
      CreateFile(device_path_.value().c_str(),
                 GENERIC_READ | GENERIC_WRITE,
                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                 NULL,
                 OPEN_EXISTING,
                 FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                 NULL));
  if (!device_handle.IsValid()) {
    Error(error::kOpenDevice);
    return false;
  }

  STORAGE_PROPERTY_QUERY query = STORAGE_PROPERTY_QUERY();
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;
  DWORD bytes_returned;

  auto output_buf = base::HeapArray<char>::Uninit(kStorageQueryBufferSize);
  BOOL status = DeviceIoControl(
      device_handle.Get(),             // Device handle.
      IOCTL_STORAGE_QUERY_PROPERTY,    // Flag to request device properties.
      &query,                          // Query parameters.
      sizeof(STORAGE_PROPERTY_QUERY),  // query parameters size.
      output_buf.data(),               // output buffer.
      kStorageQueryBufferSize,         // Size of buffer.
      &bytes_returned,                 // Number of bytes returned.
                                       // Must not be null.
      NULL);                           // Optional unused overlapped perameter.

  if (!status) {
    PLOG(ERROR) << "Storage property query failed";
    return false;
  }

  STORAGE_DEVICE_DESCRIPTOR* device_descriptor =
      reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(output_buf.data());

  return device_descriptor->RemovableMedia == TRUE ||
         device_descriptor->BusType == BusTypeUsb;
}

bool ImageWriter::OpenDevice() {
  // Windows requires that device files be opened with FILE_FLAG_NO_BUFFERING
  // and FILE_FLAG_WRITE_THROUGH.  These two flags are not part of base::File.
  device_file_ =
      base::File(CreateFile(device_path_.value().c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_READ | FILE_SHARE_WRITE,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH,
                            NULL));
  return device_file_.IsValid();
}

void ImageWriter::UnmountVolumes(base::OnceClosure continuation) {
  if (!InitializeFiles()) {
    return;
  }

  STORAGE_DEVICE_NUMBER sdn = {0};
  DWORD bytes_returned;

  BOOL status = DeviceIoControl(
      device_file_.GetPlatformFile(),
      IOCTL_STORAGE_GET_DEVICE_NUMBER,
      NULL,             // Unused, must be NULL.
      0,                // Unused, must be 0.
      &sdn,             // An input buffer to hold the STORAGE_DEVICE_NUMBER
      sizeof(sdn),      // The size of the input buffer.
      &bytes_returned,  // the actual number of bytes returned.
      NULL);            // Unused overlap.
  if (!status) {
    PLOG(ERROR) << "Unable to get device number.";
    return;
  }

  ULONG device_number = sdn.DeviceNumber;

  TCHAR volume_path[MAX_PATH + 1];
  HANDLE volume_finder = FindFirstVolume(volume_path, MAX_PATH + 1);
  if (volume_finder == INVALID_HANDLE_VALUE) {
    return;
  }

  HANDLE volume_handle;
  bool first_volume = true;
  bool success = true;

  while (first_volume ||
         FindNextVolume(volume_finder, volume_path, MAX_PATH + 1)) {
    first_volume = false;

    size_t length = wcsnlen(volume_path, MAX_PATH + 1);
    if (length < 1) {
      continue;
    }
    volume_path[length - 1] = L'\0';

    volume_handle = CreateFile(volume_path,
                               GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL,
                               OPEN_EXISTING,
                               0,
                               NULL);
    if (volume_handle == INVALID_HANDLE_VALUE) {
      PLOG(ERROR) << "Opening volume handle failed.";
      success = false;
      break;
    }

    volume_handles_.push_back(volume_handle);

    VOLUME_DISK_EXTENTS disk_extents = {0};
    status = DeviceIoControl(volume_handle,
                             IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                             NULL,
                             0,
                             &disk_extents,
                             sizeof(disk_extents),
                             &bytes_returned,
                             NULL);

    if (!status) {
      DWORD error = GetLastError();
      if (error == ERROR_MORE_DATA || error == ERROR_INVALID_FUNCTION ||
          error == ERROR_NOT_READY) {
        continue;
      } else {
        PLOG(ERROR) << "Unable to get volume disk extents.";
        success = false;
        break;
      }
    }

    if (disk_extents.NumberOfDiskExtents != 1 ||
        disk_extents.Extents[0].DiskNumber != device_number) {
      continue;
    }

    status = DeviceIoControl(volume_handle,
                             FSCTL_LOCK_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytes_returned,
                             NULL);
    if (!status) {
      PLOG(ERROR) << "Unable to lock volume.";
      success = false;
      break;
    }

    status = DeviceIoControl(volume_handle,
                             FSCTL_DISMOUNT_VOLUME,
                             NULL,
                             0,
                             NULL,
                             0,
                             &bytes_returned,
                             NULL);
    if (!status) {
      DWORD error = GetLastError();
      if (error != ERROR_NOT_SUPPORTED) {
        PLOG(ERROR) << "Unable to dismount volume.";
        success = false;
        break;
      }
    }
  }

  if (volume_finder != INVALID_HANDLE_VALUE) {
    FindVolumeClose(volume_finder);
  }

  if (success)
    std::move(continuation).Run();
}

}  // namespace image_writer
