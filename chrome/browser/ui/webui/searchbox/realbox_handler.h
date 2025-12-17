// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SEARCHBOX_REALBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SEARCHBOX_REALBOX_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "components/omnibox/browser/omnibox_popup_selection.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// Handles bidirectional communication between NTP realbox JS and the browser.
class RealboxHandler : public ContextualSearchboxHandler {
 public:
  RealboxHandler(
      mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      GetSessionHandleCallback get_session_callback);

  RealboxHandler(const RealboxHandler&) = delete;
  RealboxHandler& operator=(const RealboxHandler&) = delete;

  ~RealboxHandler() override;

  void OnThumbnailRemoved() override {}

  void UpdateSelection(OmniboxPopupSelection old_selection,
                       OmniboxPopupSelection selection);

 private:
  base::WeakPtrFactory<RealboxHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_SEARCHBOX_REALBOX_HANDLER_H_
