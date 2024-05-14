// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/machine_id_provider.h"

#include <windows.h>

#include <stdint.h>
#include <winioctl.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_handle.h"

namespace metrics {

// static
bool MachineIdProvider::HasId() {
  return true;
}

// On windows, the machine id is based on the serial number of the drive Chrome
// is running from.
// static
std::string MachineIdProvider::GetMachineId() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  // Use the program's path to get the drive used for the machine id. This means
  // that whenever the underlying drive changes, it's considered a new machine.
  // This is fine as we do not support migrating Chrome installs to new drives.
  base::FilePath executable_path;

  if (!base::PathService::Get(base::FILE_EXE, &executable_path)) {
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }

  std::vector<base::FilePath::StringType> path_components =
      executable_path.GetComponents();
  if (path_components.empty()) {
    NOTREACHED_IN_MIGRATION();
    return std::string();
  }
  base::FilePath::StringType drive_name = L"\\\\.\\" + path_components[0];

  base::win::ScopedHandle drive_handle(
      CreateFile(drive_name.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
                 nullptr, OPEN_EXISTING, 0, nullptr));

  STORAGE_PROPERTY_QUERY query = {};
  query.PropertyId = StorageDeviceProperty;
  query.QueryType = PropertyStandardQuery;

  // Perform an initial query to get the number of bytes being returned.
  DWORD bytes_returned;
  STORAGE_DESCRIPTOR_HEADER header = {};
  BOOL status = DeviceIoControl(
      drive_handle.Get(), IOCTL_STORAGE_QUERY_PROPERTY, &query,
      sizeof(STORAGE_PROPERTY_QUERY), &header,
      sizeof(STORAGE_DESCRIPTOR_HEADER), &bytes_returned, nullptr);

  if (!status)
    return std::string();

  // Query for the actual serial number.
  std::vector<int8_t> output_buf(header.Size);
  status =
      DeviceIoControl(drive_handle.Get(), IOCTL_STORAGE_QUERY_PROPERTY, &query,
                      sizeof(STORAGE_PROPERTY_QUERY), &output_buf[0],
                      output_buf.size(), &bytes_returned, nullptr);

  if (!status)
    return std::string();

  const STORAGE_DEVICE_DESCRIPTOR* device_descriptor =
      reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(&output_buf[0]);

  // The serial number is stored in the |output_buf| as a null-terminated
  // string starting at the specified offset.
  const DWORD offset = device_descriptor->SerialNumberOffset;
  if (offset >= output_buf.size())
    return std::string();

  // Make sure that the null-terminator exists.
  const std::vector<int8_t>::iterator serial_number_begin =
      output_buf.begin() + offset;
  const std::vector<int8_t>::iterator null_location =
      std::find(serial_number_begin, output_buf.end(), '\0');
  if (null_location == output_buf.end())
    return std::string();

  const char* serial_number =
      reinterpret_cast<const char*>(&output_buf[offset]);

  return std::string(serial_number);
}
}  //  namespace metrics
