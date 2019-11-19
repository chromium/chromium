// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CHROME_CLEANER_OS_FILE_REMOVER_H_
#define CHROME_CHROME_CLEANER_OS_FILE_REMOVER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "chrome/chrome_cleaner/logging/proto/removal_status.pb.h"
#include "chrome/chrome_cleaner/os/digest_verifier.h"
#include "chrome/chrome_cleaner/os/file_path_set.h"
#include "chrome/chrome_cleaner/os/file_remover_api.h"
#include "chrome/chrome_cleaner/os/layered_service_provider_api.h"
#include "chrome/chrome_cleaner/zip_archiver/zip_archiver.h"

namespace chrome_cleaner {

// This class implements the |FileRemoverAPI| for production code.
class FileRemover : public FileRemoverAPI {
 public:
  typedef base::OnceCallback<void(QuarantineStatus)> QuarantineResultCallback;

  // |digest_verifier| can be either nullptr or an instance of DigestVerifier.
  // If it is an instance of DigestVerifier, any files known to the
  // DigestVerifier will not be removed.
  FileRemover(scoped_refptr<DigestVerifier> digest_verifier,
              std::unique_ptr<ZipArchiver> archiver,
              const LayeredServiceProviderAPI& lsp,
              base::RepeatingClosure reboot_needed_callback);

  ~FileRemover() override;

  // FileRemoverAPI implementation.
  void RemoveNow(const base::FilePath& path,
                 DoneCallback callback) const override;
  void RegisterPostRebootRemoval(const base::FilePath& file_path,
                                 DoneCallback callback) const override;

  // Checks if the file is active and can be deleted.
  DeletionValidationStatus CanRemove(const base::FilePath& file) const override;

 private:
  using RemovalCallback = base::OnceCallback<
      void(const base::FilePath&, DoneCallback, QuarantineStatus)>;

  void TryToQuarantine(const base::FilePath& path,
                       QuarantineResultCallback callback) const;

  void RemoveFile(const base::FilePath& path,
                  DoneCallback removal_done_callback,
                  QuarantineStatus result) const;

  void ScheduleRemoval(const base::FilePath& file_path,
                       DoneCallback removal_done_callback,
                       QuarantineStatus quarantine_status) const;

  void ValidateAndQuarantineFile(const base::FilePath& file_path,
                                 RemovalCallback removal_callback,
                                 DoneCallback removal_done_callback) const;

  scoped_refptr<DigestVerifier> digest_verifier_;
  std::unique_ptr<ZipArchiver> archiver_;
  FilePathSet deletion_forbidden_paths_;
  base::RepeatingClosure reboot_needed_callback_;
};

}  // namespace chrome_cleaner

#endif  // CHROME_CHROME_CLEANER_OS_FILE_REMOVER_H_
