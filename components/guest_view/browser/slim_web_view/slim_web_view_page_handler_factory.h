// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PAGE_HANDLER_FACTORY_H_
#define COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PAGE_HANDLER_FACTORY_H_

#include "components/guest_view/browser/slim_web_view/slim_web_view.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class RenderFrameHost;
}

namespace guest_view {

class SlimWebViewPageHandlerFactory : public mojom::PageHandlerFactory {
 public:
  SlimWebViewPageHandlerFactory();
  ~SlimWebViewPageHandlerFactory() override;

  void BindInterface(mojo::PendingReceiver<mojom::PageHandlerFactory> receiver);

 protected:
  void CreatePageHandler(mojo::PendingReceiver<mojom::PageHandler> receiver,
                         mojo::PendingRemote<mojom::Page> page) final;
  virtual content::RenderFrameHost* GetWebUiRenderFrameHost() = 0;

 private:
  mojo::Receiver<mojom::PageHandlerFactory> receiver_{this};
};

}  // namespace guest_view

#endif  // COMPONENTS_GUEST_VIEW_BROWSER_SLIM_WEB_VIEW_SLIM_WEB_VIEW_PAGE_HANDLER_FACTORY_H_
