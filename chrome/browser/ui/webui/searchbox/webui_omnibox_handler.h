// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class MetricsReporter;
class OmniboxController;
class Profile;

namespace content {
class WebContents;
}  // namespace content

// Handles bidirectional communication between NTP realbox JS and the browser.
class WebuiOmniboxHandler : public SearchboxHandler {
 public:
  WebuiOmniboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter,
      OmniboxController* omnibox_controller);

  WebuiOmniboxHandler(const WebuiOmniboxHandler&) = delete;
  WebuiOmniboxHandler& operator=(const WebuiOmniboxHandler&) = delete;

  ~WebuiOmniboxHandler() override;

  void OnThumbnailRemoved() override {}

  void UpdateSelection(OmniboxPopupSelection old_selection,
                       OmniboxPopupSelection selection);

 private:

  base::WeakPtrFactory<WebuiOmniboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_HANDLER_H_
