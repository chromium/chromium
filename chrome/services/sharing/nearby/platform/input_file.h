// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_FILE_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_FILE_H_

#include "base/files/file.h"
#include "third_party/nearby/src/internal/platform/implementation/input_file.h"

namespace nearby::chrome {

// Concrete InputFile implementation.
class InputFile : public api::InputFile {
 public:
  explicit InputFile(base::File file);
  ~InputFile() override;

  InputFile(const InputFile&) = delete;
  InputFile& operator=(const InputFile&) = delete;

  // api::InputFile:
  std::string GetFilePath() const override;
  std::int64_t GetTotalSize() const override;
  ExceptionOr<ByteArray> Read(std::int64_t size) override;
  Exception Close() override;

  // Extract the underlying base::File.
  base::File ExtractUnderlyingFile();

 private:
  // File::GetLength is not const but api::InputFile::GetTotalSize is const.
  mutable base::File file_;
};

}  // namespace nearby::chrome

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_INPUT_FILE_H_
