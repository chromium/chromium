// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/file_util_service.h"

#include <memory>

#include "base/bind.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/services/file_util/safe_archive_analyzer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/services/file_util/zip_file_creator.h"
#endif

FileUtilService::FileUtilService(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver)
    : receiver_(this, std::move(receiver)) {}

FileUtilService::~FileUtilService() = default;

#if defined(OS_CHROMEOS)
void FileUtilService::BindZipFileCreator(
    mojo::PendingReceiver<chrome::mojom::ZipFileCreator> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<chrome::ZipFileCreator>(),
                              std::move(receiver));
}
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
void FileUtilService::BindSafeArchiveAnalyzer(
    mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SafeArchiveAnalyzer>(),
                              std::move(receiver));
}
#endif
