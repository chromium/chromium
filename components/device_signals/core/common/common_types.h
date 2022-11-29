// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device_signals {

// Used to indicate whether a given signal was correctly found or not, or
// indicate a reason for not being able to find it.
enum class PresenceValue { kUnspecified, kAccessDenied, kNotFound, kFound };

struct ExecutableMetadata {
  ExecutableMetadata();

  ExecutableMetadata(const ExecutableMetadata&);
  ExecutableMetadata& operator=(const ExecutableMetadata&);

  ~ExecutableMetadata();

  // Is true if a currently running process was spawned from this file.
  bool is_running = false;

  // Byte strings containing the SHA-256 hash of the DER-encoded SPKI structures
  // of the certificates used to sign the executable.
  absl::optional<std::vector<std::string>> public_keys_hashes = absl::nullopt;

  // Product name of this executable.
  absl::optional<std::string> product_name = absl::nullopt;

  // Version of this executable.
  absl::optional<std::string> version = absl::nullopt;

  bool operator==(const ExecutableMetadata& other) const;
};

struct FileSystemItem {
  FileSystemItem();

  FileSystemItem(const FileSystemItem&);
  FileSystemItem& operator=(const FileSystemItem&);

  ~FileSystemItem();

  // Path to the file system object for whom those signals were collected.
  base::FilePath file_path{};

  // Value indicating whether the specific resource could be found or not.
  PresenceValue presence = PresenceValue::kUnspecified;

  // Byte string containing the SHA256 hash of a file’s bytes. Ignored when
  // `path` points to a directory. Collected only when `compute_sha256` is set
  // to true in the corresponding GetFileSystemInfoOptions parameter.
  absl::optional<std::string> sha256_hash = absl::nullopt;

  // Set of properties only relevant for executable files. Will only be
  // collected if computeIsExecutable is set to true in the given signals
  // collection parameters and if `path` points to an executable file.
  absl::optional<ExecutableMetadata> executable_metadata = absl::nullopt;

  bool operator==(const FileSystemItem& other) const;
};

struct GetFileSystemInfoOptions {
  GetFileSystemInfoOptions();

  GetFileSystemInfoOptions(const GetFileSystemInfoOptions&);
  GetFileSystemInfoOptions& operator=(const GetFileSystemInfoOptions&);

  ~GetFileSystemInfoOptions();

  base::FilePath file_path{};

  bool compute_sha256 = false;

  bool compute_executable_metadata = false;

  bool operator==(const GetFileSystemInfoOptions& other) const;
};

struct CrowdStrikeSignals {
  std::string customer_id;
  std::string agent_id;

  // Returns a Value with the non-empty values. Returns absl::nullopt if neither
  // values are set.
  absl::optional<base::Value> ToValue() const;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_
