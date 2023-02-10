// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/fake_document_analysis_service.h"

FakeDocumentAnalysisService::FakeDocumentAnalysisService(
    mojo::PendingReceiver<chrome::mojom::DocumentAnalysisService> receiver)
    : receiver_(this, std::move(receiver)) {}

FakeDocumentAnalysisService::~FakeDocumentAnalysisService() = default;

MockSafeDocumentAnalyzer&
FakeDocumentAnalysisService::GetSafeDocumentAnalyzer() {
  return safe_document_analyzer_;
}

#if BUILDFLAG(ENABLE_MALDOCA)
void FakeDocumentAnalysisService::BindSafeDocumentAnalyzer(
    mojo::PendingReceiver<chrome::mojom::SafeDocumentAnalyzer> receiver) {
  safe_document_analyzer_.Bind(std::move(receiver));
}
#endif

MockSafeDocumentAnalyzer::MockSafeDocumentAnalyzer() = default;
MockSafeDocumentAnalyzer::~MockSafeDocumentAnalyzer() = default;
void MockSafeDocumentAnalyzer::Bind(
    mojo::PendingReceiver<chrome::mojom::SafeDocumentAnalyzer> receiver) {
  receivers_.Add(this, std::move(receiver));
}
