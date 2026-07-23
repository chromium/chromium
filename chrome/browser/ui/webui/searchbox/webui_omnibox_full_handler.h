// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_FULL_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_FULL_HANDLER_H_

#include <memory>

#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// A minimal implementation of ContextualSearchboxHandler that doesn't use an
// OmniboxController or OmniboxEditModel.
class WebuiOmniboxFullHandler : public ContextualSearchboxHandler {
 public:
  explicit WebuiOmniboxFullHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_page,
      Profile* profile,
      content::WebContents* web_contents,
      std::unique_ptr<OmniboxClient> client,
      GetSessionHandleCallback get_session_callback);

  WebuiOmniboxFullHandler(const WebuiOmniboxFullHandler&) = delete;
  WebuiOmniboxFullHandler& operator=(const WebuiOmniboxFullHandler&) = delete;

  ~WebuiOmniboxFullHandler() override;

  // searchbox::mojom::PageHandler:
  void OnThumbnailRemoved() override {}
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_WEBUI_OMNIBOX_FULL_HANDLER_H_
