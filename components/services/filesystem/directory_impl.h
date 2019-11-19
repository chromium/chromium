// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_FILESYSTEM_DIRECTORY_IMPL_H_
#define COMPONENTS_SERVICES_FILESYSTEM_DIRECTORY_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "components/services/filesystem/shared_temp_dir.h"

namespace filesystem {

class LockTable;

class DirectoryImpl : public mojom::Directory {
 public:
  // Set |temp_dir| only if there's a temporary directory that should be deleted
  // when this object is destroyed.
  DirectoryImpl(base::FilePath directory_path,
                scoped_refptr<SharedTempDir> temp_dir,
                scoped_refptr<LockTable> lock_table);
  ~DirectoryImpl() override;

  // |Directory| implementation:
  void Read(ReadCallback callback) override;
  void OpenFile(const std::string& path,
                mojo::PendingReceiver<mojom::File> receiver,
                uint32_t open_flags,
                OpenFileCallback callback) override;
  void OpenFileHandle(const std::string& path,
                      uint32_t open_flags,
                      OpenFileHandleCallback callback) override;
  void OpenFileHandles(std::vector<mojom::FileOpenDetailsPtr> details,
                       OpenFileHandlesCallback callback) override;
  void OpenDirectory(const std::string& path,
                     mojo::PendingReceiver<mojom::Directory> receiver,
                     uint32_t open_flags,
                     OpenDirectoryCallback callback) override;
  void Rename(const std::string& path,
              const std::string& new_path,
              RenameCallback callback) override;
  void Replace(const std::string& path,
               const std::string& new_path,
               ReplaceCallback callback) override;
  void Delete(const std::string& path,
              uint32_t delete_flags,
              DeleteCallback callback) override;
  void Exists(const std::string& path, ExistsCallback callback) override;
  void IsWritable(const std::string& path,
                  IsWritableCallback callback) override;
  void Flush(FlushCallback callback) override;
  void StatFile(const std::string& path, StatFileCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Directory> receiver) override;
  void ReadEntireFile(const std::string& path,
                      ReadEntireFileCallback callback) override;
  void WriteFile(const std::string& path,
                 const std::vector<uint8_t>& data,
                 WriteFileCallback callback) override;

 private:
  base::File OpenFileHandleImpl(const std::string& raw_path,
                                uint32_t open_flags);

  base::FilePath directory_path_;
  scoped_refptr<SharedTempDir> temp_dir_;
  scoped_refptr<LockTable> lock_table_;

  DISALLOW_COPY_AND_ASSIGN(DirectoryImpl);
};

}  // namespace filesystem

#endif  // COMPONENTS_SERVICES_FILESYSTEM_DIRECTORY_IMPL_H_
