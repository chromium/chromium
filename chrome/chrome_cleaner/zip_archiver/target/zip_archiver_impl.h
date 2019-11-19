// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TARGET_ZIP_ARCHIVER_IMPL_H_
#define CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TARGET_ZIP_ARCHIVER_IMPL_H_

#include <string>

#include "chrome/chrome_cleaner/mojom/zip_archiver.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chrome_cleaner {

class ZipArchiverImpl : public mojom::ZipArchiver {
 public:
  ZipArchiverImpl(mojo::PendingReceiver<mojom::ZipArchiver> receiver,
                  base::OnceClosure connection_error_handler);
  ~ZipArchiverImpl() override;

  void Archive(mojo::ScopedHandle src_file_handle,
               mojo::ScopedHandle zip_file_handle,
               const std::string& filename_in_zip,
               const std::string& password,
               ArchiveCallback callback) override;

 private:
  mojo::Receiver<mojom::ZipArchiver> receiver_;

  DISALLOW_COPY_AND_ASSIGN(ZipArchiverImpl);
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_ZIP_ARCHIVER_TARGET_ZIP_ARCHIVER_IMPL_H_
