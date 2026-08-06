// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class OmniboxEverywhereService;
class MetricsReporter;

namespace content {
class WebUI;
}

// Custom handler for OmniboxEverywhere searchbox.
// It owns its own OmniboxController and OmniboxEverywhereClient.
class OmniboxEverywhereHandler : public ContextualSearchboxHandler {
 public:
  OmniboxEverywhereHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      MetricsReporter* metrics_reporter,
      content::WebUI* web_ui,
      OmniboxEverywhereService* service,
      GetSessionHandleCallback get_session_callback);

  OmniboxEverywhereHandler(const OmniboxEverywhereHandler&) = delete;
  OmniboxEverywhereHandler& operator=(const OmniboxEverywhereHandler&) = delete;

  ~OmniboxEverywhereHandler() override;

  // searchbox::mojom::PageHandler:
  void OnThumbnailRemoved() override {}

  // Overridden to intercept the Drive upload request, dynamically associate the
  // standalone WebContents with the latest active BrowserWindowInterface, and
  // update the OmniboxEverywhereService state.
  void OnDriveUploadClicked(OnDriveUploadClickedCallback callback) override;
  void OpenProfilePicker() override;

  // ContextualSearchboxHandler:
  // Overridden to notify the OmniboxEverywhereService when the Drive picker is
  // dismissed, allowing the standalone widget to regain activation and focus.
  void CleanupDrivePicker() override;

 private:
  raw_ptr<OmniboxEverywhereService> service_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_HANDLER_H_
