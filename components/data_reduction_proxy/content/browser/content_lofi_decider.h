// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_CONTENT_LOFI_DECIDER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_CONTENT_LOFI_DECIDER_H_

#include "components/data_reduction_proxy/core/common/lofi_decider.h"

#include "base/macros.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "content/public/common/previews_state.h"

namespace net {
class HttpRequestHeaders;
class URLRequest;
}

namespace data_reduction_proxy {

// Class responsible for deciding whether a request should be requested with low
// fidelity (Lo-Fi) or not. Relies on the Lo-Fi mode state stored in the
// request's content::ResourceRequestInfo, which must be fetched using
// content::ResourceRequestInfo::ForRequest. Lo-Fi mode will not be enabled for
// requests that don't have a ResourceRequestInfo, such as background requests.
// Owned by DataReductionProxyIOData and should be called on the IO thread.
class ContentLoFiDecider : public LoFiDecider {
 public:
  ContentLoFiDecider();
  ~ContentLoFiDecider() override;

  // Returns an updated PreviewsState with respect to server previews
  // given the main frame's committed |request| and the |initial_state|
  // of enabled previews. |data| must have already been updated with
  // respect to the main frame response headers.
  static content::PreviewsState DetermineCommittedServerPreviewsState(
      DataReductionProxyData* data,
      content::PreviewsState initial_state);

  // LoFiDecider implementation:
  void MaybeSetAcceptTransformHeader(
      const net::URLRequest& request,
      net::HttpRequestHeaders* headers) const override;
  void RemoveAcceptTransformHeader(
      net::HttpRequestHeaders* headers) const override;
  bool IsClientLoFiImageRequest(const net::URLRequest& request) const override;
  bool IsClientLoFiAutoReloadRequest(
      const net::URLRequest& request) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentLoFiDecider);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CONTENT_BROWSER_CONTENT_LOFI_DECIDER_H_
