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
      profile_(profile) {}

ContentAnnotatorInternalsPageHandler::~ContentAnnotatorInternalsPageHandler() =
    default;

void ContentAnnotatorInternalsPageHandler::GetAnnotatedContent(
    GetAnnotatedContentCallback callback) {
  accessibility_annotator::AccessibilityAnnotatorBackend* backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile_);
  if (!backend) {
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(backend->GetDebugUIFormattedCacheData());
}

}  // namespace content_annotator_internals
