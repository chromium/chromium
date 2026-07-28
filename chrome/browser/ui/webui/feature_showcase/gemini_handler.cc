// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/feature_showcase/gemini_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/profiles/feature_showcase/feature_showcase_metrics.h"
#include "components/glic/glic_pref_names.h"
#include "components/prefs/pref_service.h"

GeminiHandler::GeminiHandler(
    mojo::PendingReceiver<feature_showcase::mojom::GeminiPageHandler> receiver,
    glic::GlicKeyedService* glic_service)
    : receiver_(this, std::move(receiver)),
      glic_service_(CHECK_DEREF(glic_service)) {}

GeminiHandler::~GeminiHandler() = default;

void GeminiHandler::AcceptConsent() {
  glic_service_->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  RecordStepUserAction(FeatureShowcaseStep::kGemini,
                       FeatureShowcaseStepUserAction::kAccepted);
  // TODO(crbug.com/506845213): Record Gemini specific metrics if applicable.
}

void GeminiHandler::SkipConsent() {
  glic_service_->enabling().SetCompletedFre(
      glic::prefs::FreStatus::kIncomplete);
  RecordStepUserAction(FeatureShowcaseStep::kGemini,
                       FeatureShowcaseStepUserAction::kDeclined);
  // TODO(crbug.com/506845213): Record Gemini specific metrics if applicable.
}
