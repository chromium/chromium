// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_TEST_PRINT_TEST_CONTENT_RENDERER_CLIENT_H_
#define COMPONENTS_PRINTING_TEST_PRINT_TEST_CONTENT_RENDERER_CLIENT_H_

#include "content/public/renderer/content_renderer_client.h"

namespace printing {

class PrintTestContentRendererClient : public content::ContentRendererClient {
 public:
  PrintTestContentRendererClient();
  ~PrintTestContentRendererClient() override;

  static void SetGenerateTaggedPDFs(bool generate);

  // content::ContentRendererClient:
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_TEST_PRINT_TEST_CONTENT_RENDERER_CLIENT_H_
