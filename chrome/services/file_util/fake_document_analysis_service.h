// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_FAKE_DOCUMENT_ANALYSIS_SERVICE_H_
#define CHROME_SERVICES_FILE_UTIL_FAKE_DOCUMENT_ANALYSIS_SERVICE_H_

#include "base/files/file.h"
#include "build/build_config.h"
#include "chrome/services/file_util/buildflags.h"
#include "chrome/services/file_util/public/mojom/document_analysis_service.mojom.h"
#include "chrome/services/file_util/public/mojom/safe_document_analyzer.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gmock/include/gmock/gmock.h"

// An implementation of the SafeDocumentAnalyzer interface that delegates all
// the Mojo methods to mocks.
class MockSafeDocumentAnalyzer : public chrome::mojom::SafeDocumentAnalyzer {
 public:
  MockSafeDocumentAnalyzer();

  MockSafeDocumentAnalyzer(const MockSafeDocumentAnalyzer&) = delete;
  MockSafeDocumentAnalyzer& operator=(const MockSafeDocumentAnalyzer&) = delete;

  ~MockSafeDocumentAnalyzer() override;

  void Bind(
      mojo::PendingReceiver<chrome::mojom::SafeDocumentAnalyzer> receiver);

  MOCK_METHOD(void,
              AnalyzeDocument,
              (base::File office_file,
               const base::FilePath& file_path,
               AnalyzeDocumentCallback callback),
              (override));

 private:
  mojo::ReceiverSet<chrome::mojom::SafeDocumentAnalyzer> receivers_;
};

// An implementation of chrome::mojom::DocumentAnalysisService that binds and
// exposes mock interfaces, for use in tests.
class FakeDocumentAnalysisService
    : public chrome::mojom::DocumentAnalysisService {
 public:
  explicit FakeDocumentAnalysisService(
      mojo::PendingReceiver<chrome::mojom::DocumentAnalysisService> receiver);

  FakeDocumentAnalysisService(const FakeDocumentAnalysisService&) = delete;
  FakeDocumentAnalysisService& operator=(const FakeDocumentAnalysisService&) =
      delete;

  ~FakeDocumentAnalysisService() override;

  MockSafeDocumentAnalyzer& GetSafeDocumentAnalyzer();

 private:
  // chrome::mojom::DocumentAnalysisService implementation
#if BUILDFLAG(ENABLE_MALDOCA)
  void BindSafeDocumentAnalyzer(
      mojo::PendingReceiver<chrome::mojom::SafeDocumentAnalyzer> receiver)
      override;
#endif

  mojo::Receiver<chrome::mojom::DocumentAnalysisService> receiver_;
  MockSafeDocumentAnalyzer safe_document_analyzer_;
};

#endif  // CHROME_SERVICES_FILE_UTIL_FAKE_DOCUMENT_ANALYSIS_SERVICE_H_
