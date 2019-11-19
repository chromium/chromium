// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_IME_DECODER_DOWNLOADER_IMPL_H_
#define CHROMEOS_SERVICES_IME_DECODER_DOWNLOADER_IMPL_H_

#include "base/macros.h"
#include "chromeos/services/ime/public/cpp/shared_lib/interfaces.h"

namespace chromeos {
namespace ime {

class DownloaderImpl : public ImeCrosDownloader {
 public:
  explicit DownloaderImpl();
  ~DownloaderImpl() override;

  int DownloadToFile(const char* url,
                     const DownloadOptions& options,
                     const char* file_path,
                     ImeCrosDownloadCallback callback) override;

  void Cancel(int request_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloaderImpl);
};

}  // namespace ime
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_IME_DECODER_DOWNLOADER_IMPL_H_
