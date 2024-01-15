// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_FAKE_FILE_UTIL_SERVICE_H_
#define CHROME_SERVICES_FILE_UTIL_FAKE_FILE_UTIL_SERVICE_H_

#include <optional>

#include "base/files/file.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/services/file_util/buildflags.h"
#include "chrome/services/file_util/public/mojom/file_util_service.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/services/file_util/public/mojom/safe_archive_analyzer.mojom.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
// An implementation of the SafeArchiveAnalyzer interface that delegates all the
// Mojo methods to mocks.
class MockSafeArchiveAnalyzer : public chrome::mojom::SafeArchiveAnalyzer {
 public:
  MockSafeArchiveAnalyzer();

  MockSafeArchiveAnalyzer(const MockSafeArchiveAnalyzer&) = delete;
  MockSafeArchiveAnalyzer& operator=(const MockSafeArchiveAnalyzer&) = delete;

  ~MockSafeArchiveAnalyzer() override;

  void Bind(mojo::PendingReceiver<chrome::mojom::SafeArchiveAnalyzer> receiver);

  MOCK_METHOD(
      void,
      AnalyzeZipFile,
      (base::File zip_file,
       const std::optional<std::string>& password,
       mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
       AnalyzeZipFileCallback callback),
      (override));
  MOCK_METHOD(
      void,
      AnalyzeDmgFile,
      (base::File dmg_file,
       mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
       AnalyzeDmgFileCallback callback),
      (override));
  MOCK_METHOD(
      void,
      AnalyzeRarFile,
      (base::File rar_file,
       const std::optional<std::string>& password,
       mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
       AnalyzeRarFileCallback callback),
      (override));
  MOCK_METHOD(
      void,
      AnalyzeSevenZipFile,
      (base::File seven_zip_file,
       mojo::PendingRemote<chrome::mojom::TemporaryFileGetter> temp_file_getter,
       AnalyzeSevenZipFileCallback callback),
      (override));

 private:
  mojo::ReceiverSet<chrome::mojom::SafeArchiveAnalyzer> receivers_;
};
#endif

// An implementation of chrome::mojom::FileUtilService that binds and exposes
// mock interfaces, for use in tests.
class FakeFileUtilService : public chrome::mojom::FileUtilService {
 public:
  explicit FakeFileUtilService(
      mojo::PendingReceiver<chrome::mojom::FileUtilService> receiver);

  FakeFileUtilService(const FakeFileUtilService&) = delete;
  FakeFileUtilService& operator=(const FakeFileUtilService&) = delete;

  ~FakeFileUtilService() override;

#if BUILDFLAG(FULL_SAFE_BROWSING)
  MockSafeArchiveAnalyzer& GetSafeArchiveAnalyzer();
#endif

 private:
  // chrome::mojom::FileUtilService implementation
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

#if BUILDFLAG(FULL_SAFE_BROWSING)
  MockSafeArchiveAnalyzer safe_archive_analyzer_;
#endif
};

#endif  // CHROME_SERVICES_FILE_UTIL_FAKE_FILE_UTIL_SERVICE_H_
