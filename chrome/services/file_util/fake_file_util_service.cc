// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/fake_file_util_service.h"

FakeFileUtilService::FakeFileUtilService(
    mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeFileUtilService::~FakeFileUtilService() = default;

#if BUILDFLAG(FULL_SAFE_BROWSING)
MockSafeArchiveAnalyzer& FakeFileUtilService::GetSafeArchiveAnalyzer() {
  return safe_archive_analyzer_;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void FakeFileUtilService::BindZipFileCreator(
    mojo::PendingReceiver<chrome::mojom::ZipFileCreator> receiver) {
  NOTREACHED_IN_MIGRATION();
}
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
void FakeFileUtilService::BindSafeArchiveAnalyzer(
    mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver) {
  safe_archive_analyzer_.Bind(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTRACTORS)
void FakeFileUtilService::BindSingleFileTarXzFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  NOTREACHED_IN_MIGRATION();
}

void FakeFileUtilService::BindSingleFileTarFileExtractor(
    mojo::PendingReceiver<chrome::mojom::SingleFileExtractor> receiver) {
  NOTREACHED_IN_MIGRATION();
}
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
MockSafeArchiveAnalyzer::MockSafeArchiveAnalyzer() = default;
MockSafeArchiveAnalyzer::~MockSafeArchiveAnalyzer() = default;
void MockSafeArchiveAnalyzer::Bind(
    mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver) {
  receivers_.Add(this, std::move(receiver));
}
#endif
