// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_SCOPED_TEMP_FILE_H_
#define CHROMECAST_BASE_SCOPED_TEMP_FILE_H_

#include <string>

#include "base/files/file_path.h"

namespace chromecast {

// Creates a temporary file that is deleted when this object is destroyed,
// unless the underlying file has been moved or deleted.
// Warning: This class uses CHECKs, and should only be used for testing.
class ScopedTempFile {
 public:
  ScopedTempFile();

  ScopedTempFile(const ScopedTempFile&) = delete;
  ScopedTempFile& operator=(const ScopedTempFile&) = delete;

  ~ScopedTempFile();

  // Return the path to the temporary file. Note that if the underlying file has
  // been moved or deleted, this will still return the original path.
  base::FilePath path() const { return path_; }

  // Returns true if the underlying file exists, false otherwise. This will
  // return false, for example, if the file has been moved or deleted.
  bool FileExists() const;

  // Write the contents of |str| to the file. Returns the whether all the
  // contents were written to the file. CHECKs that FileExists() returns true.
  bool Write(const std::string& str);

  // Read the file and return the contents. CHECKs that FileExists() returns
  // true.
  std::string Read() const;

 private:
  base::FilePath path_;
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_SCOPED_TEMP_FILE_H_
