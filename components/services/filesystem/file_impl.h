// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FILESYSTEM_FILE_IMPL_H_
#define COMPONENTS_SERVICES_FILESYSTEM_FILE_IMPL_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"

namespace base {
class FilePath;
}

namespace filesystem {

class LockTable;
class SharedTempDir;

class FileImpl : public mojom::File {
 public:
  FileImpl(const base::FilePath& path,
           uint32_t flags,
           scoped_refptr<SharedTempDir> temp_dir,
           scoped_refptr<LockTable> lock_table);
  FileImpl(const base::FilePath& path,
           base::File file,
           scoped_refptr<SharedTempDir> temp_dir,
           scoped_refptr<LockTable> lock_table);
  ~FileImpl() override;

  // Returns whether the underlying file handle is valid.
  bool IsValid() const;

#if !defined(OS_FUCHSIA)
  // Attempts to perform the native operating system's locking operations on
  // the internal mojom::File handle. Not supported on Fuchsia.
  base::File::Error RawLockFile();
  base::File::Error RawUnlockFile();
#endif  // !OS_FUCHSIA

  const base::FilePath& path() const { return path_; }

  // |File| implementation:
  void Close(CloseCallback callback) override;
  void Read(uint32_t num_bytes_to_read,
            int64_t offset,
            mojom::Whence whence,
            ReadCallback callback) override;
  void Write(const std::vector<uint8_t>& bytes_to_write,
             int64_t offset,
             mojom::Whence whence,
             WriteCallback callback) override;
  void Tell(TellCallback callback) override;
  void Seek(int64_t offset,
            mojom::Whence whence,
            SeekCallback callback) override;
  void Stat(StatCallback callback) override;
  void Truncate(int64_t size, TruncateCallback callback) override;
  void Touch(mojom::TimespecOrNowPtr atime,
             mojom::TimespecOrNowPtr mtime,
             TouchCallback callback) override;
  void Dup(mojo::PendingReceiver<mojom::File> receiver,
           DupCallback callback) override;
  void Flush(FlushCallback callback) override;
  void Lock(LockCallback callback) override;
  void Unlock(UnlockCallback callback) override;
  void AsHandle(AsHandleCallback callback) override;

 private:
  base::File file_;
  base::FilePath path_;
  scoped_refptr<SharedTempDir> temp_dir_;
  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(FileImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_SERVICES_FILESYSTEM_FILE_IMPL_H_
