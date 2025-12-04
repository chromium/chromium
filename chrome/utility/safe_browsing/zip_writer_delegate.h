// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SAFE_BROWSING_ZIP_WRITER_DELEGATE_H_
#define CHROME_UTILITY_SAFE_BROWSING_ZIP_WRITER_DELEGATE_H_

#include "base/files/file.h"
#include "third_party/zlib/google/zip_reader.h"

namespace safe_browsing {

class SafeBrowsingZipWriterDelegate : public zip::WriterDelegate {
 public:
  virtual bool has_disk_error() const = 0;
  virtual int64_t file_length() const = 0;
  virtual void Close() {}
};

}  // namespace safe_browsing

#endif  // CHROME_UTILITY_SAFE_BROWSING_ZIP_WRITER_DELEGATE_H_
