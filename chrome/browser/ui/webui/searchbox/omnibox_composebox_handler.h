// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_OMNIBOX_COMPOSEBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_OMNIBOX_COMPOSEBOX_HANDLER_H_

#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// ComposeboxHandler for the Omnibox Popup.
class OmniboxComposeboxHandler : public ComposeboxHandler {
 public:
  OmniboxComposeboxHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      Profile* profile,
      content::WebContents* web_contents);

  ~OmniboxComposeboxHandler() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_OMNIBOX_COMPOSEBOX_HANDLER_H_
