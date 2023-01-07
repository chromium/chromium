// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_TEST_PRINT_TEST_CONTENT_RENDERER_CLIENT_H_
#define COMPONENTS_PRINTING_TEST_PRINT_TEST_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace printing {

class PrintTestContentRendererClient : public content::ContentRendererClient {
 public:
  explicit PrintTestContentRendererClient(bool generate_tagged_pdfs);
  ~PrintTestContentRendererClient() override;

  // content::ContentRendererClient:
  void RenderFrameCreated(content::RenderFrame* render_frame) override;

 private:
  const bool generate_tagged_pdfs_;
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_TEST_PRINT_TEST_CONTENT_RENDERER_CLIENT_H_
