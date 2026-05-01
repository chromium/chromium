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
  NotifyPageWithAnnotations();
}

void ContentAnnotatorInternalsPageHandler::OnContentAnnotationsDeleted(
    base::span<const history::VisitID> visit_ids) {
  NotifyPageWithAnnotations();
}

void ContentAnnotatorInternalsPageHandler::OnContentAnnotationsCleared() {
  page_->OnContentAnnotationsCleared();
}

void ContentAnnotatorInternalsPageHandler::NotifyPageWithAnnotations() {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    return;
  }
  backend->GetAnnotationsForDebugUI(base::BindOnce(
      [](base::WeakPtr<ContentAnnotatorInternalsPageHandler> handler,
         base::Value data) {
        if (handler) {
          handler->page_->OnContentAnnotationsChanged(std::move(data));
        }
      },
      weak_ptr_factory_.GetWeakPtr()));
}

void ContentAnnotatorInternalsPageHandler::GetAnnotatedContent(
    GetAnnotatedContentCallback callback) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    std::move(callback).Run(base::Value());
    return;
  }
  backend->GetAnnotationsForDebugUI(std::move(callback));
}

void ContentAnnotatorInternalsPageHandler::ClearAnnotatedContent(
    ClearAnnotatedContentCallback callback) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    std::move(callback).Run(false);
    return;
  }
  backend->ClearAllContentAnnotations(std::move(callback));
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
  backend->DeleteContentAnnotations(visit_ids, std::move(callback));
}

}  // namespace content_annotator_internals
