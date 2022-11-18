// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_FILE_UTIL_SERVICE_H_
#define CHROME_SERVICES_FILE_UTIL_FILE_UTIL_SERVICE_H_

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/file_util/buildflags.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class FileUtilService : public chrome::mojom::FileUtilService {
 public:
  explicit FileUtilService(
      mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver);

  FileUtilService(const FileUtilService&) = delete;
  FileUtilService& operator=(const FileUtilService&) = delete;

  ~FileUtilService() override;

 private:
  // chrome::mojom::FileUtilService implementation:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void BindZipFileCreator(
      mojo::PendingReceiver<chrome::mojom::ZipFileCreator> receiver) override;
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  void BindSafeArchiveAnalyzer(
      mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver)
      override;
#endif

#if BUILDFLAG(ENABLE_EXTRACTORS)
  void BindSingleFileTarXzFileExtractor(
      mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver)
      override;
  void BindSingleFileTarFileExtractor(
      mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver)
      override;
#endif

  mojo::Receiver<chrome::mojom::FileUtilService> receiver_;
};

#endif  // CHROME_SERVICES_FILE_UTIL_FILE_UTIL_SERVICE_H_
