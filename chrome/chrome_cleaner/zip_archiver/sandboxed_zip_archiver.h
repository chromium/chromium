// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ZIP_ARCHIVER_SANDBOXED_ZIP_ARCHIVER_H_
#define CHROME_CHROME_CLEANER_ZIP_ARCHIVER_SANDBOXED_ZIP_ARCHIVER_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "chrome/chrome_cleaner/ipc/mojo_task_runner.h"
#include "chrome/chrome_cleaner/ipc/sandbox.h"
#include "chrome/chrome_cleaner/zip_archiver/broker/sandbox_setup.h"
#include "chrome/chrome_cleaner/zip_archiver/zip_archiver.h"

namespace chrome_cleaner {

namespace internal {

base::string16 ConstructZipArchiveFileName(const base::string16& filename,
                                           const std::string& file_hash,
                                           size_t max_filename_length);

}  // namespace internal

class SandboxedZipArchiver : public ZipArchiver {
 public:
  SandboxedZipArchiver(scoped_refptr<MojoTaskRunner> mojo_task_runner,
                       RemoteZipArchiverPtr zip_archiver,
                       const base::FilePath& dst_archive_folder,
                       const std::string& zip_password);
  ~SandboxedZipArchiver() override;

  void Archive(const base::FilePath& src_file_path,
               ArchiveResultCallback result_callback) override;

 private:
  mojom::ZipArchiverResultCode CheckFileSize(base::File* file);

  scoped_refptr<MojoTaskRunner> mojo_task_runner_;
  RemoteZipArchiverPtr zip_archiver_;
  const base::FilePath dst_archive_folder_;
  const std::string zip_password_;
  size_t dst_max_component_length_;
};

ResultCode SpawnZipArchiverSandbox(
    const base::FilePath& dst_archive_folder,
    const std::string& zip_password,
    scoped_refptr<MojoTaskRunner> mojo_task_runner,
    const SandboxConnectionErrorCallback& connection_error_callback,
    std::unique_ptr<SandboxedZipArchiver>* sandboxed_zip_archiver);

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ZIP_ARCHIVER_SANDBOXED_ZIP_ARCHIVER_H_
