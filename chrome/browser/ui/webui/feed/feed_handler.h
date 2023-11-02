// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_FEED_FEED_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_FEED_FEED_HANDLER_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/feed/feed.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/mojom/base/string16.mojom.h"

namespace feed {

class FeedHandler : public feed::mojom::FeedSidePanelHandler {
 public:
  FeedHandler(
      mojo::PendingReceiver<feed::mojom::FeedSidePanelHandler> pending_receiver,
      mojo::PendingRemote<feed::mojom::FeedSidePanel> pending_side_panel);
  ~FeedHandler() override;
  FeedHandler(const FeedHandler&) = delete;
  FeedHandler& operator=(const FeedHandler&) = delete;
  // Handles the page requesting an action from the controller.
  void DoSomething() override;

 private:
  // Called as a reaction to DoSomething; invoked when DoSomething is done.
  void OnSomethingDone();
  mojo::Receiver<feed::mojom::FeedSidePanelHandler> receiver_;
  mojo::Remote<feed::mojom::FeedSidePanel> side_panel_;
  base::WeakPtrFactory<FeedHandler> weak_ptr_factory_{this};
};

}  // namespace feed

#endif  // CHROME_BROWSER_UI_WEBUI_FEED_FEED_HANDLER_H_
