// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_handler.h"

#include <utility>

#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"

GeminiHandler::GeminiHandler(
    mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> receiver)
    : receiver_(this, std::move(receiver)) {}

GeminiHandler::~GeminiHandler() = default;

void GeminiHandler::AcceptConsent() {
  // TODO(crbug.com/506845213): Implement logic for granting consent.
  RecordStepUserAction(FeatureShowcaseStep::kGemini,
                       FeatureShowcaseStepUserAction::kAccepted);
}

void GeminiHandler::SkipConsent() {
  RecordStepUserAction(FeatureShowcaseStep::kGemini,
                       FeatureShowcaseStepUserAction::kDeclined);
}
