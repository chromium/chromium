// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_

#include "chrome/browser/ui/webui/memories/memories.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

// Handles bidirectional communication between memories page and the browser.
class MemoriesHandler : public memories::mojom::PageHandler {
 public:
  MemoriesHandler(
      mojo::PendingReceiver<memories::mojom::PageHandler> pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents);
  ~MemoriesHandler() override;

  MemoriesHandler(const MemoriesHandler&) = delete;
  MemoriesHandler& operator=(const MemoriesHandler&) = delete;

  // memories::mojom::PageHandler:
  void SetPage(
      mojo::PendingRemote<memories::mojom::Page> pending_page) override;

 private:
  Profile* profile_;
  content::WebContents* web_contents_;

  mojo::Remote<memories::mojom::Page> page_;
  mojo::Receiver<memories::mojom::PageHandler> page_handler_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORIES_MEMORIES_HANDLER_H_
