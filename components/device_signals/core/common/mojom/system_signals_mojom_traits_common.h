// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MOJOM_SYSTEM_SIGNALS_MOJOM_TRAITS_COMMON_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MOJOM_SYSTEM_SIGNALS_MOJOM_TRAITS_COMMON_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "components/device_signals/core/common/common_types.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom-shared.h"

namespace mojo {

template <>
struct EnumTraits<device_signals::mojom::PresenceValue,
                  device_signals::PresenceValue> {
  static device_signals::mojom::PresenceValue ToMojom(
      device_signals::PresenceValue input);
  static bool FromMojom(device_signals::mojom::PresenceValue input,
                        device_signals::PresenceValue* output);
};

template <>
struct StructTraits<device_signals::mojom::ExecutableMetadataDataView,
                    device_signals::ExecutableMetadata> {
  static bool is_running(const device_signals::ExecutableMetadata& input) {
    return input.is_running;
  }

  static std::optional<std::vector<std::string>> public_keys_hashes(
      const device_signals::ExecutableMetadata& input) {
    return input.public_keys_hashes;
  }

  static std::optional<std::string> product_name(
      const device_signals::ExecutableMetadata& input) {
    return input.product_name;
  }

  static std::optional<std::string> version(
      const device_signals::ExecutableMetadata& input) {
    return input.version;
  }

  static bool is_os_verified(const device_signals::ExecutableMetadata& input) {
    return input.is_os_verified;
  }

  static std::optional<std::string> subject_name(
      const device_signals::ExecutableMetadata& input) {
    return input.subject_name;
  }

  static bool Read(device_signals::mojom::ExecutableMetadataDataView data,
                   device_signals::ExecutableMetadata* output);
};

template <>
struct StructTraits<device_signals::mojom::FileSystemItemDataView,
                    device_signals::FileSystemItem> {
  static const base::FilePath& file_path(
      const device_signals::FileSystemItem& input) {
    return input.file_path;
  }

  static device_signals::PresenceValue presence(
      const device_signals::FileSystemItem& input) {
    return input.presence;
  }

  static std::optional<std::string> sha256_hash(
      const device_signals::FileSystemItem& input) {
    return input.sha256_hash;
  }

  static std::optional<device_signals::ExecutableMetadata> executable_metadata(
      const device_signals::FileSystemItem& input) {
    return input.executable_metadata;
  }

  static bool Read(device_signals::mojom::FileSystemItemDataView input,
                   device_signals::FileSystemItem* output);
};

template <>
struct StructTraits<device_signals::mojom::FileSystemItemRequestDataView,
                    device_signals::GetFileSystemInfoOptions> {
  static const base::FilePath& file_path(
      const device_signals::GetFileSystemInfoOptions& input) {
    return input.file_path;
  }

  static bool compute_sha256(
      const device_signals::GetFileSystemInfoOptions& input) {
    return input.compute_sha256;
  }

  static bool compute_executable_metadata(
      const device_signals::GetFileSystemInfoOptions& input) {
    return input.compute_executable_metadata;
  }

  static bool Read(device_signals::mojom::FileSystemItemRequestDataView input,
                   device_signals::GetFileSystemInfoOptions* output);
};

}  // namespace mojo

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_MOJOM_SYSTEM_SIGNALS_MOJOM_TRAITS_COMMON_H_
