// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;

namespace content {
class WebContents;
}

class ContextHubPageHandler : public browser::context_hub::mojom::PageHandler {
 public:
  explicit ContextHubPageHandler(
      mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
      Profile* profile,
      content::WebContents* web_contents);
  ~ContextHubPageHandler() override;

  ContextHubPageHandler(const ContextHubPageHandler&) = delete;
  ContextHubPageHandler& operator=(const ContextHubPageHandler&) = delete;

 private:
  mojo::Receiver<browser::context_hub::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONTEXT_HUB_CONTEXT_HUB_PAGE_HANDLER_H_
