// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/common/mojom/system_signals_mojom_traits.h"

#include "base/notreached.h"
#include "mojo/public/cpp/base/byte_string_mojom_traits.h"
#include "mojo/public/cpp/base/file_path_mojom_traits.h"

namespace mojo {

// static
device_signals::mojom::PresenceValue
EnumTraits<device_signals::mojom::PresenceValue,
           device_signals::PresenceValue>::ToMojom(device_signals::PresenceValue
                                                       input) {
  switch (input) {
    case device_signals::PresenceValue::kUnspecified:
      return device_signals::mojom::PresenceValue::kUnspecified;
    case device_signals::PresenceValue::kAccessDenied:
      return device_signals::mojom::PresenceValue::kAccessDenied;
    case device_signals::PresenceValue::kNotFound:
      return device_signals::mojom::PresenceValue::kNotFound;
    case device_signals::PresenceValue::kFound:
      return device_signals::mojom::PresenceValue::kFound;
  }
}

// static
device_signals::PresenceValue EnumTraits<device_signals::mojom::PresenceValue,
                                         device_signals::PresenceValue>::
    FromMojom(device_signals::mojom::PresenceValue input) {
  switch (input) {
    case device_signals::mojom::PresenceValue::kUnspecified:
      return device_signals::PresenceValue::kUnspecified;
    case device_signals::mojom::PresenceValue::kAccessDenied:
      return device_signals::PresenceValue::kAccessDenied;
    case device_signals::mojom::PresenceValue::kNotFound:
      return device_signals::PresenceValue::kNotFound;
    case device_signals::mojom::PresenceValue::kFound:
      return device_signals::PresenceValue::kFound;
  }

  NOTREACHED();
}

// static
bool StructTraits<device_signals::mojom::ExecutableMetadataDataView,
                  device_signals::ExecutableMetadata>::
    Read(device_signals::mojom::ExecutableMetadataDataView data,
         device_signals::ExecutableMetadata* output) {
  output->is_running = data.is_running();
  output->is_os_verified = data.is_os_verified();

  if (!data.ReadPublicKeysHashes(&output->public_keys_hashes) ||
      !data.ReadProductName(&output->product_name) ||
      !data.ReadVersion(&output->version) ||
      !data.ReadSubjectName(&output->subject_name)) {
    return false;
  }

  return true;
}

// static
bool StructTraits<device_signals::mojom::FileSystemItemDataView,
                  device_signals::FileSystemItem>::
    Read(device_signals::mojom::FileSystemItemDataView data,
         device_signals::FileSystemItem* output) {
  if (!data.ReadFilePath(&output->file_path) ||
      !data.ReadPresence(&output->presence) ||
      !data.ReadSha256Hash(&output->sha256_hash) ||
      !data.ReadExecutableMetadata(&output->executable_metadata)) {
    return false;
  }

  return true;
}

// static
bool StructTraits<device_signals::mojom::FileSystemItemRequestDataView,
                  device_signals::GetFileSystemInfoOptions>::
    Read(device_signals::mojom::FileSystemItemRequestDataView data,
         device_signals::GetFileSystemInfoOptions* output) {
  output->compute_sha256 = data.compute_sha256();
  output->compute_executable_metadata = data.compute_executable_metadata();

  if (!data.ReadFilePath(&output->file_path)) {
    return false;
  }

  return true;
}

}  // namespace mojo
