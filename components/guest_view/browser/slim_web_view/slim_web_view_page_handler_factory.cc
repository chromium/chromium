// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/browser/slim_web_view/slim_web_view_page_handler_factory.h"

#include "components/guest_view/browser/slim_web_view/slim_web_view_page_handler.h"

namespace guest_view {

SlimWebViewPageHandlerFactory::SlimWebViewPageHandlerFactory() = default;
SlimWebViewPageHandlerFactory::~SlimWebViewPageHandlerFactory() = default;

void SlimWebViewPageHandlerFactory::BindInterface(
    mojo::PendingReceiver<mojom::PageHandlerFactory> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void SlimWebViewPageHandlerFactory::CreatePageHandler(
    mojo::PendingReceiver<mojom::PageHandler> receiver,
    mojo::PendingRemote<mojom::Page> page) {
  SlimWebViewPageHandler::CreateForCurrentDocument(
      GetWebUiRenderFrameHost(), std::move(receiver), std::move(page));
}

}  // namespace guest_view
