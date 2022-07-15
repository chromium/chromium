// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_

#include <string>

#include "base/files/file_path.h"
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

  // Is true if the file for which this payload was generated is indeed an
  // executable. If this is false, all of the other properties will be
  // absl::nullopt.
  bool is_executable = false;

  // Is true if a currently running process was spawned from this file.
  absl::optional<bool> is_running = absl::nullopt;

  // SHA256 hash of the public key of the certificate used to sign the
  // executable.
  absl::optional<std::string> public_key_sha256 = absl::nullopt;

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

  // SHA256 hash of a file’s bytes. Ignored when `path` points to a
  // directory. Collected only when `compute_sha256` is set to true in the
  // corresponding GetFileSystemInfoOptions parameter.
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

  bool compute_is_executable = false;

  bool operator==(const GetFileSystemInfoOptions& other) const;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_
