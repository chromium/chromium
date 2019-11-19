// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/content/browser/contextual_search_js_api_service_impl.h"

#include <memory>
#include <utility>

#include "components/contextual_search/content/browser/contextual_search_js_api_handler.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace contextual_search {

ContextualSearchJsApiServiceImpl::ContextualSearchJsApiServiceImpl(
    ContextualSearchJsApiHandler* contextual_search_js_api_handler)
    : contextual_search_js_api_handler_(contextual_search_js_api_handler) {}

ContextualSearchJsApiServiceImpl::~ContextualSearchJsApiServiceImpl() {}

void ContextualSearchJsApiServiceImpl::ShouldEnableJsApi(
    const GURL& gurl,
    mojom::ContextualSearchJsApiService::ShouldEnableJsApiCallback callback) {
  contextual_search_js_api_handler_->ShouldEnableJsApi(gurl,
                                                       std::move(callback));
}

void ContextualSearchJsApiServiceImpl::HandleSetCaption(
    const std::string& caption,
    bool does_answer) {
  contextual_search_js_api_handler_->SetCaption(caption, does_answer);
}

void ContextualSearchJsApiServiceImpl::HandleChangeOverlayPosition(
    mojom::OverlayPosition desired_position) {
  contextual_search_js_api_handler_->ChangeOverlayPosition(desired_position);
}

// static
void CreateContextualSearchJsApiService(
    ContextualSearchJsApiHandler* contextual_search_js_api_handler,
    mojo::PendingReceiver<mojom::ContextualSearchJsApiService> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<ContextualSearchJsApiServiceImpl>(
          contextual_search_js_api_handler),
      std::move(receiver));
}

}  // namespace contextual_search
