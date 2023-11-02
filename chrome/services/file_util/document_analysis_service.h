// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_FILE_UTIL_DOCUMENT_ANALYSIS_SERVICE_H_
#define CHROME_SERVICES_FILE_UTIL_DOCUMENT_ANALYSIS_SERVICE_H_

#include "build/build_config.h"
#include "chrome/services/file_util/buildflags.h"
#include "chrome/services/file_util/public/mojom/document_analysis_service.mojom.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class DocumentAnalysisService : public chrome::mojom::DocumentAnalysisService {
 public:
  explicit DocumentAnalysisService(
      mojo::PendingReceiver<chrome::mojom::DocumentAnalysisService> receiver);
  ~DocumentAnalysisService() override;

  DocumentAnalysisService(const DocumentAnalysisService&) = delete;
  DocumentAnalysisService& operator=(const DocumentAnalysisService&) = delete;

 private:
  // chrome::mojom::DocumentAnalysisService implementation:
#if BUILDFLAG(ENABLE_MALDOCA)
  void BindSafeDocumentAnalyzer(
      mojo::PendingReceiver<chrome::mojom::SafeDocumentAnalyzer> receiver)
      override;
#endif

  mojo::Receiver<chrome::mojom::DocumentAnalysisService> receiver_;
};

#endif  // CHROME_SERVICES_FILE_UTIL_DOCUMENT_ANALYSIS_SERVICE_H_
