// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/content/browser/content_lofi_decider.h"

#include <string>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/data_reduction_proxy/content/common/header_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_headers.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/previews/core/previews_decider.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/common/resource_type.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "url/gurl.h"

namespace data_reduction_proxy {

ContentLoFiDecider::ContentLoFiDecider() {}

ContentLoFiDecider::~ContentLoFiDecider() {}

// Static
content::PreviewsState
ContentLoFiDecider::DetermineCommittedServerPreviewsState(
    DataReductionProxyData* data,
    content::PreviewsState initial_state) {
  if (!data) {
    return initial_state &=
           ~(content::SERVER_LITE_PAGE_ON | content::SERVER_LOFI_ON);
  }
  content::PreviewsState updated_state = initial_state;
  if (!data->lite_page_received()) {
    // Turn off LitePage bit.
    updated_state &= ~(content::SERVER_LITE_PAGE_ON);
  }
  if (!data->lofi_policy_received()) {
    // Turn off LoFi bit(s).
    updated_state &= ~(content::SERVER_LOFI_ON);
    if (data->used_data_reduction_proxy()) {
      // Turn off Client LoFi bit also if using proxy but proxy did not
      // request LoFi.
      updated_state &= ~(content::CLIENT_LOFI_ON);
    }
  }
  return updated_state;
}

void ContentLoFiDecider::MaybeSetAcceptTransformHeader(
    const net::URLRequest& request,
    net::HttpRequestHeaders* headers) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);

  if (!request_info)
    return;

  content::ResourceType resource_type = request_info->GetResourceType();
  content::PreviewsState previews_state = request_info->GetPreviewsState();
  ::data_reduction_proxy::MaybeSetAcceptTransformHeader(
      request.url(), resource_type, previews_state, headers);
}

void ContentLoFiDecider::RemoveAcceptTransformHeader(
    net::HttpRequestHeaders* headers) const {
  headers->RemoveHeader(chrome_proxy_accept_transform_header());
}

bool ContentLoFiDecider::IsClientLoFiImageRequest(
    const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  return request_info &&
         request_info->GetResourceType() == content::RESOURCE_TYPE_IMAGE &&
         (request_info->GetPreviewsState() & content::CLIENT_LOFI_ON);
}

bool ContentLoFiDecider::IsClientLoFiAutoReloadRequest(
    const net::URLRequest& request) const {
  const content::ResourceRequestInfo* request_info =
      content::ResourceRequestInfo::ForRequest(&request);
  return request_info &&
         (request_info->GetPreviewsState() & content::CLIENT_LOFI_AUTO_RELOAD);
}

}  // namespace data_reduction_proxy
