// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_COMPOSEBOX_EVERYWHERE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_COMPOSEBOX_EVERYWHERE_HANDLER_H_

#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

class Profile;

namespace content {
class WebContents;
}

// Custom handler for OmniboxEverywhere composebox.
// It owns its own OmniboxController and EverywhereComposeboxClient.
class ComposeboxEverywhereHandler : public ComposeboxHandler {
 public:
  ComposeboxEverywhereHandler(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      Profile* profile,
      content::WebContents* web_contents,
      GetSessionHandleCallback get_session_callback,
      ClearSessionHandleCallback clear_session_callback);

  ComposeboxEverywhereHandler(const ComposeboxEverywhereHandler&) = delete;
  ComposeboxEverywhereHandler& operator=(const ComposeboxEverywhereHandler&) =
      delete;

  ~ComposeboxEverywhereHandler() override;
};

#endif  // CHROME_BROWSER_UI_WEBUI_OMNIBOX_EVERYWHERE_COMPOSEBOX_EVERYWHERE_HANDLER_H_
