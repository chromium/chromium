// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_

#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "components/omnibox/browser/omnibox.mojom.h"

// Browser-side handler for bidirectional communication with the WebUI
// contextual searchbox.
class ContextualSearchboxHandler : public SearchboxHandler {
 public:
  ContextualSearchboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      MetricsReporter* metrics_reporter,
      OmniboxController* omnibox_controller);

  ~ContextualSearchboxHandler() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_CONTEXTUAL_SEARCHBOX_HANDLER_H_
