// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_FILE_UTIL_SERVICE_H_
#define CHROME_SERVICES_FILE_UTIL_FILE_UTIL_SERVICE_H_

#include "build/build_config.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class FileUtilService : public chrome::mojom::FileUtilService {
 public:
  explicit FileUtilService(
      mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver);
  ~FileUtilService() override;

 private:
  // chrome::mojom::FileUtilService implementation:
#if defined(OS_CHROMEOS)
  void BindZipFileCreator(
      mojo::PendingReceiver<chrome::mojom::ZipFileCreator> receiver) override;
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  void BindSafeArchiveAnalyzer(
      mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver)
      override;
#endif

  mojo::Receiver<chrome::mojom::FileUtilService> receiver_;

  DISALLOW_COPY_AND_ASSIGN(FileUtilService);
};

#endif  // CHROME_SERVICES_FILE_UTIL_FILE_UTIL_SERVICE_H_
