// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals_page_handler.h"

#include "chrome/browser/profiles/profile.h"

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

// TODO(crbug.com/529716749): Remove this function when removing the internals
// page.
void PersonalContextInternalsPageHandler::TriggerFirstRun(
    TriggerFirstRunCallback callback) {
  std::move(callback).Run(false);
}
