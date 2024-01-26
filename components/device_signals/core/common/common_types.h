// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"

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
  std::optional<std::vector<std::string>> public_keys_hashes = std::nullopt;

  // Product name of this executable.
  std::optional<std::string> product_name = std::nullopt;

  // Version of this executable.
  std::optional<std::string> version = std::nullopt;

  // Is true if the OS has verified the signing certificate.
  bool is_os_verified = false;

  // The subject name of the signing certificate.
  std::optional<std::string> subject_name = std::nullopt;

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
  std::optional<std::string> sha256_hash = std::nullopt;

  // Set of properties only relevant for executable files. Will only be
  // collected if computeIsExecutable is set to true in the given signals
  // collection parameters and if `path` points to an executable file.
  std::optional<ExecutableMetadata> executable_metadata = std::nullopt;

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
  std::string customer_id{};
  std::string agent_id{};

  // Returns a Value with the non-empty values. Returns std::nullopt if neither
  // values are set.
  std::optional<base::Value> ToValue() const;

  bool operator==(const CrowdStrikeSignals& other) const;
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_COMMON_COMMON_TYPES_H_
