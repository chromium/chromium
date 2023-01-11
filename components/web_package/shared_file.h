// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_PACKAGE_SHARED_FILE_H_
#define COMPONENTS_WEB_PACKAGE_SHARED_FILE_H_

#include <memory>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"

namespace web_package {

// A simple wrapper class to share a single `base::File` instance among multiple
// `SharedFile::SharedFileDataSource` instances.
class SharedFile final : public base::RefCountedThreadSafe<SharedFile> {
 public:
  // The file passed to this method must have been opened for reading.
  explicit SharedFile(std::unique_ptr<base::File> file);

  SharedFile(const SharedFile&) = delete;
  SharedFile& operator=(const SharedFile&) = delete;

  // Duplicate the underlying `base::File`. This is only needed for APIs that
  // require a `base::File` and cannot use a `SharedFile`.
  void DuplicateFile(base::OnceCallback<void(base::File)> callback);

  // Allow access to the underlying instance of `base::File`.
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
    // Implements `mojo::DataPipeProducer::DataSource`. The following methods
    // are called on a blockable sequenced task runner.
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

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::File> file_;
};

}  // namespace web_package

#endif  // COMPONENTS_WEB_PACKAGE_SHARED_FILE_H_
