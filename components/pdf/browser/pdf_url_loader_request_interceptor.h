// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_BROWSER_PDF_URL_LOADER_REQUEST_INTERCEPTOR_H_
#define COMPONENTS_PDF_BROWSER_PDF_URL_LOADER_REQUEST_INTERCEPTOR_H_

#include <memory>

#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/url_loader_request_interceptor.h"

namespace pdf {

class PdfStreamDelegate;

class PdfURLLoaderRequestInterceptor final
    : public content::URLLoaderRequestInterceptor {
 public:
  static std::unique_ptr<content::URLLoaderRequestInterceptor>
  MaybeCreateInterceptor(content::FrameTreeNodeId frame_tree_node_id,
                         std::unique_ptr<PdfStreamDelegate> stream_delegate);

  PdfURLLoaderRequestInterceptor(
      content::FrameTreeNodeId frame_tree_node_id,
      std::unique_ptr<PdfStreamDelegate> stream_delegate);
  PdfURLLoaderRequestInterceptor(const PdfURLLoaderRequestInterceptor&) =
      delete;
  PdfURLLoaderRequestInterceptor& operator=(
      const PdfURLLoaderRequestInterceptor&) = delete;
  ~PdfURLLoaderRequestInterceptor() override;

  // `content::URLLoaderRequestInterceptor`:
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      content::BrowserContext* browser_context,
      content::URLLoaderRequestInterceptor::LoaderCallback callback) override;

 private:
  RequestHandler CreateRequestHandler(
      const network::ResourceRequest& tentative_resource_request);

  content::FrameTreeNodeId frame_tree_node_id_;
  std::unique_ptr<PdfStreamDelegate> stream_delegate_;
};

}  // namespace pdf

#endif  // COMPONENTS_PDF_BROWSER_PDF_URL_LOADER_REQUEST_INTERCEPTOR_H_
