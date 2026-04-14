// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator_internals/accessibility_annotator_internals_page_handler.h"

#include "chrome/browser/accessibility_annotator/first_run/accessibility_annotator_first_run_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_service.h"

AccessibilityAnnotatorInternalsPageHandler::
    AccessibilityAnnotatorInternalsPageHandler(
        mojo::PendingReceiver<
            browser::accessibility_annotator_internals::mojom::PageHandler>
            receiver,
        Profile* profile,
        content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_contents_(web_contents) {}

AccessibilityAnnotatorInternalsPageHandler::
    ~AccessibilityAnnotatorInternalsPageHandler() = default;

void AccessibilityAnnotatorInternalsPageHandler::TriggerFirstRun(
    TriggerFirstRunCallback callback) {
  auto* service =
      AccessibilityAnnotatorFirstRunServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->MaybeTriggerFirstRun(
      web_contents_,
      accessibility_annotator::FirstRunInvocationSource::kAutoTriggerPromo,
      base::BindOnce(
          [](TriggerFirstRunCallback callback,
             accessibility_annotator::FirstRunTriggerResult result) {
            std::move(callback).Run(
                result ==
                accessibility_annotator::FirstRunTriggerResult::kSuccess);
          },
          std::move(callback)));
}
