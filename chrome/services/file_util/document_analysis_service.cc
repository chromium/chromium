// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/file_util/document_analysis_service.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "build/build_config.h"
#include "chrome/services/file_util/buildflags.h"
#include "components/safe_browsing/buildflags.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

#if BUILDFLAG(ENABLE_MALDOCA)
#include "chrome/services/file_util/safe_document_analyzer.h"
#endif

DocumentAnalysisService::DocumentAnalysisService(
    mojo::PendingReceiver<chrome::mojom::DocumentAnalysisService> receiver)
    : receiver_(this, std::move(receiver)) {}

DocumentAnalysisService::~DocumentAnalysisService() = default;

#if BUILDFLAG(ENABLE_MALDOCA)
void DocumentAnalysisService::BindSafeDocumentAnalyzer(
    mojo::PendingReceiver<chrome::mojom::SafeDocumentAnalyzer> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<SafeDocumentAnalyzer>(),
                              std::move(receiver));
}
#endif
