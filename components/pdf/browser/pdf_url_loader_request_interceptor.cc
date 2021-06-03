// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/browser/pdf_url_loader_request_interceptor.h"

#include <utility>

#include "base/callback.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/url_loader_request_interceptor.h"
#include "services/network/public/cpp/resource_request.h"

namespace pdf {

PdfURLLoaderRequestInterceptor::PdfURLLoaderRequestInterceptor() = default;
PdfURLLoaderRequestInterceptor::~PdfURLLoaderRequestInterceptor() = default;

void PdfURLLoaderRequestInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    content::BrowserContext* browser_context,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  std::move(callback).Run({});
}

}  // namespace pdf
