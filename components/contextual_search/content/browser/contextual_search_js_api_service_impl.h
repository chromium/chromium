// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_BROWSER_CONTEXTUAL_SEARCH_JS_API_SERVICE_IMPL_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_BROWSER_CONTEXTUAL_SEARCH_JS_API_SERVICE_IMPL_H_

#include "base/macros.h"
#include "components/contextual_search/content/browser/contextual_search_js_api_handler.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace contextual_search {

// This is the receiving end of Contextual Search JavaScript API calls.
// TODO(donnd): Move this to java.  See https://crbug.com/866972.
class ContextualSearchJsApiServiceImpl
    : public mojom::ContextualSearchJsApiService {
 public:
  explicit ContextualSearchJsApiServiceImpl(
      ContextualSearchJsApiHandler* contextual_search_js_api_handler);
  ~ContextualSearchJsApiServiceImpl() override;

  // Mojo ContextualSearchApiService implementation.
  // Determines if the JavaScript API should be enabled for the given |gurl|.
  // The given |callback| will be notified with the answer.
  void ShouldEnableJsApi(
      const GURL& gurl,
      mojom::ContextualSearchJsApiService::ShouldEnableJsApiCallback callback)
      override;

  // Handles a JavaScript call to set the caption in the Bar to
  // the given |message|.
  void HandleSetCaption(const std::string& message, bool does_answer) override;

  // Handles a JavaScript call to change the Overlay position.
  // The panel cannot be changed to any opened position if it's not already
  // opened.
  void HandleChangeOverlayPosition(
      mojom::OverlayPosition desired_position) override;

 private:
  // The UI handler for calls through the JavaScript API.
  ContextualSearchJsApiHandler* contextual_search_js_api_handler_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchJsApiServiceImpl);
};

// static
void CreateContextualSearchJsApiService(
    ContextualSearchJsApiHandler* contextual_search_js_api_handler,
    mojo::PendingReceiver<mojom::ContextualSearchJsApiService> receiver);

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_BROWSER_CONTEXTUAL_SEARCH_JS_API_SERVICE_IMPL_H_
