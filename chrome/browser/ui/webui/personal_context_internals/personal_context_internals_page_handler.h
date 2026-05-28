// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}

class PersonalContextInternalsPageHandler
    : public browser::personal_context_internals::mojom::PageHandler {
 public:
  explicit PersonalContextInternalsPageHandler(
      mojo::PendingReceiver<
          browser::personal_context_internals::mojom::PageHandler> receiver,
      Profile* profile,
      content::WebContents* web_contents);
  ~PersonalContextInternalsPageHandler() override;

  PersonalContextInternalsPageHandler(
      const PersonalContextInternalsPageHandler&) = delete;
  PersonalContextInternalsPageHandler& operator=(
      const PersonalContextInternalsPageHandler&) = delete;

  // browser::personal_context_internals::mojom::PageHandler:
  void TriggerFirstRun(TriggerFirstRunCallback callback) override;

 private:
  mojo::Receiver<browser::personal_context_internals::mojom::PageHandler>
      receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_PERSONAL_CONTEXT_INTERNALS_PERSONAL_CONTEXT_INTERNALS_PAGE_HANDLER_H_
