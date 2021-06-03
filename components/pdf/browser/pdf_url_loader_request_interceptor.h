// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_URL_LOADER_REQUEST_INTERCEPTOR_H_
#define COMPONENTS_PDF_BROWSER_PDF_URL_LOADER_REQUEST_INTERCEPTOR_H_

#include "content/public/browser/url_loader_request_interceptor.h"

namespace pdf {

class PdfURLLoaderRequestInterceptor final
    : public content::URLLoaderRequestInterceptor {
 public:
  PdfURLLoaderRequestInterceptor();
  PdfURLLoaderRequestInterceptor(const PdfURLLoaderRequestInterceptor&) =
      delete;
  PdfURLLoaderRequestInterceptor& operator=(
      const PdfURLLoaderRequestInterceptor&) = delete;
  ~PdfURLLoaderRequestInterceptor() override;

  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_URL_LOADER_REQUEST_INTERCEPTOR_H_
