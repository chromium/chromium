// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_DELEGATE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace device_signals {

struct CustomFilePathComparator {
  bool operator()(const base::FilePath& a, const base::FilePath& b) const;
};

template <typename T>
using FilePathMap = base::flat_map<base::FilePath, T, CustomFilePathComparator>;

using FilePathSet = base::flat_set<base::FilePath, CustomFilePathComparator>;

// Interface whose derived types encapsulate OS-specific functionalities.
class PlatformDelegate {
 public:
  virtual ~PlatformDelegate() = default;

  // Wrapper functions around implementation in base/files/file_util.h to allow
  // mocking in tests.
  virtual bool PathIsReadable(const base::FilePath& file_path) const = 0;
  virtual bool DirectoryExists(const base::FilePath& file_path) const = 0;

  // Resolves environment variables and relative markers in `file_path`, and
  // returns the absolute path via `resolved_file_path`. Returns true if
  // successful. For consistency on all platforms, this method will return false
  // if no file system item resides at the end path.
  virtual bool ResolveFilePath(const base::FilePath& file_path,
                               base::FilePath* resolved_file_path) = 0;

  // Returns a map of file paths to whether a currently running process was
  // spawned from that file. The set of file paths in the map are specified by
  // `file_paths`.
  virtual FilePathMap<bool> AreExecutablesRunning(
      const FilePathSet& file_paths) = 0;

  struct ProductMetadata {
    ProductMetadata();

    ProductMetadata(const ProductMetadata&);
    ProductMetadata& operator=(const ProductMetadata&);

    ~ProductMetadata();

    absl::optional<std::string> name = absl::nullopt;
    absl::optional<std::string> version = absl::nullopt;

    bool operator==(const ProductMetadata& other) const;
  };

  // Returns product metadata for a given `file_path`.
  // On Windows, this looks at file metadata.
  // On Mac, it looks for app bundle metadata.
  virtual absl::optional<ProductMetadata> GetProductMetadata(
      const base::FilePath& file_path);

  // Returns the public key SHA256 hashes of the certificates used to sign an
  // executable file located at `file_path`. Returns absl::nullopt if
  // unsupported on the current platform.
  virtual absl::optional<std::vector<std::string>>
  GetSigningCertificatesPublicKeyHashes(const base::FilePath& file_path);
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_SYSTEM_SIGNALS_PLATFORM_DELEGATE_H_
