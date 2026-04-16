// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/content_annotator_internals/content_annotator_internals_page_handler.h"

#include <string>
#include <utility>

#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/accessibility_annotator/core/logging/accessibility_annotator_internals.mojom.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"

namespace content_annotator_internals {

ContentAnnotatorInternalsPageHandler::ContentAnnotatorInternalsPageHandler(
    mojo::PendingReceiver<accessibility_annotator_internals::mojom::PageHandler>
        receiver,
    mojo::PendingRemote<accessibility_annotator_internals::mojom::Page> page,
    Profile* profile)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      profile_(profile) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (backend) {
    backend_observation_.Observe(backend);
  }
}

ContentAnnotatorInternalsPageHandler::~ContentAnnotatorInternalsPageHandler() =
    default;

void ContentAnnotatorInternalsPageHandler::OnContentAnnotationsAdded(
    history::VisitID visit_id,
    const accessibility_annotator::AccessibilityAnnotatorBackend::
        ContentAnnotationsData& annotation_data) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    return;
  }
  page_->OnContentAnnotationsAdded(backend->GetDebugUICacheData());
}

void ContentAnnotatorInternalsPageHandler::OnContentAnnotationsDeleted(
    base::span<const history::VisitID> visit_ids) {
  // TODO(crbug.com/496384941): Implement this function when data is persisted
  // and can be deleted via Chrome History / TTL.
}

void ContentAnnotatorInternalsPageHandler::OnContentAnnotationsCleared() {
  // TODO(crbug.com/496384941): Implement this function when data is persisted
  // and can be deleted via Chrome History / TTL.
}

void ContentAnnotatorInternalsPageHandler::GetAnnotatedContent(
    GetAnnotatedContentCallback callback) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    std::move(callback).Run(base::Value());
    return;
  }
  std::move(callback).Run(backend->GetDebugUICacheData());
}

void ContentAnnotatorInternalsPageHandler::ClearAnnotatedContent(
    ClearAnnotatedContentCallback callback) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    std::move(callback).Run(false);
    return;
  }
  backend->ClearContentAnnotationsCache();
  std::move(callback).Run(true);
}

void ContentAnnotatorInternalsPageHandler::DeleteAnnotatedContent(
    const std::vector<int64_t>& visit_ids,
    DeleteAnnotatedContentCallback callback) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    std::move(callback).Run(false);
    return;
  }

  backend->RemoveContentAnnotationsCacheData(visit_ids);
  std::move(callback).Run(true);
}

}  // namespace content_annotator_internals
