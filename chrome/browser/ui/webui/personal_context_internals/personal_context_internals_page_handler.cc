// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals_page_handler.h"

#include "chrome/browser/personal_context/first_run/personal_context_first_run_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/personal_context/first_run/personal_context_first_run_service.h"

PersonalContextInternalsPageHandler::PersonalContextInternalsPageHandler(
    mojo::PendingReceiver<
        browser::personal_context_internals::mojom::PageHandler> receiver,
    Profile* profile,
    content::WebContents* web_contents)
    : receiver_(this, std::move(receiver)),
      profile_(profile),
      web_contents_(web_contents) {}

PersonalContextInternalsPageHandler::~PersonalContextInternalsPageHandler() =
    default;

void PersonalContextInternalsPageHandler::TriggerFirstRun(
    TriggerFirstRunCallback callback) {
  auto* service =
      PersonalContextFirstRunServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->MaybeTriggerFirstRun(
      web_contents_,
      personal_context::FirstRunInvocationSource::kAutoTriggerPromo,
      base::BindOnce(
          [](TriggerFirstRunCallback callback,
             personal_context::FirstRunTriggerResult result) {
            std::move(callback).Run(
                result == personal_context::FirstRunTriggerResult::kSuccess);
          },
          std::move(callback)));
}
