// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SHARED_FILE_H_
#define COMPONENTS_WEB_PACKAGE_SHARED_FILE_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"

namespace web_package {

// A simple wrapper class to share a single base::File instance among multiple
// SharedFileDataSource instances.
class SharedFile final : public base::RefCountedThreadSafe<SharedFile> {
 public:
  // The callback passed to the constructor will run on a thread that allows
  // blocking disk IO.
  explicit SharedFile(
      base::OnceCallback<std::unique_ptr<base::File>()> open_file_callback);

  SharedFile(const SharedFile&) = delete;
  SharedFile& operator=(const SharedFile&) = delete;

  void DuplicateFile(base::OnceCallback<void(base::File)> callback);
  base::File* operator->();

  class SharedFileDataSource final : public mojo::DataPipeProducer::DataSource {
   public:
    SharedFileDataSource(scoped_refptr<SharedFile> file,
                         uint64_t offset,
                         uint64_t length);

    SharedFileDataSource(const SharedFileDataSource&) = delete;
    SharedFileDataSource& operator=(const SharedFileDataSource&) = delete;

    ~SharedFileDataSource() override;

   private:
    // Implements mojo::DataPipeProducer::DataSource. Following methods are
    // called on a blockable sequenced task runner.
    uint64_t GetLength() const override;
    ReadResult Read(uint64_t offset, base::span<char> buffer) override;

    scoped_refptr<SharedFile> file_;
    MojoResult error_;
    const uint64_t offset_;
    const uint64_t length_;
  };

  std::unique_ptr<SharedFileDataSource> CreateDataSource(uint64_t offset,
                                                         uint64_t length);

 private:
  friend class base::RefCountedThreadSafe<SharedFile>;
  ~SharedFile();

  void SetFile(std::unique_ptr<base::File> file);

  base::FilePath file_path_;
  std::unique_ptr<base::File> file_;
  base::OnceCallback<void(base::File)> duplicate_callback_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SHARED_FILE_H_
