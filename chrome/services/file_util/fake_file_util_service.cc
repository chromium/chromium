// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/fake_file_util_service.h"

#include "build/build_config.h"

FakeFileUtilService::FakeFileUtilService(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeFileUtilService::~FakeFileUtilService() = default;

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
MockSafeArchiveAnalyzer& FakeFileUtilService::GetSafeArchiveAnalyzer() {
  return safe_archive_analyzer_;
}
#endif

#if BUILDFLAG(IS_CHROMEOS)
void FakeFileUtilService::BindZipFileCreator(
    mojo::PendingReceiver<chrome::mojom::ZipFileCreator> receiver) {
  NOTREACHED();
}
#endif

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
void FakeFileUtilService::BindSafeArchiveAnalyzer(
    mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver) {
  safe_archive_analyzer_.Bind(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTRACTORS)
void FakeFileUtilService::BindSingleFileTarXzFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  NOTREACHED();
}

void FakeFileUtilService::BindSingleFileTarFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  NOTREACHED();
}
#endif

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
MockSafeArchiveAnalyzer::MockSafeArchiveAnalyzer() = default;
MockSafeArchiveAnalyzer::~MockSafeArchiveAnalyzer() = default;
void MockSafeArchiveAnalyzer::Bind(
    mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver) {
  receivers_.Add(this, std::move(receiver));
}
#endif
