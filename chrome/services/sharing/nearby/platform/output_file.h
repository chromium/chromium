// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_OUTPUT_FILE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_OUTPUT_FILE_H_

#include "base/files/file.h"
#include "third_party/nearby/src/internal/platform/implementation/output_file.h"

namespace nearby::chrome {

// Concrete OutputFile implementation.
class OutputFile : public api::OutputFile {
 public:
  explicit OutputFile(base::File file);
  ~OutputFile() override;

  OutputFile(const OutputFile&) = delete;
  OutputFile& operator=(const OutputFile&) = delete;

  // api::OutputFile:
  Exception Write(const ByteArray& data) override;
  Exception Flush() override;
  Exception Close() override;

 private:
  base::File file_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_OUTPUT_FILE_H_
