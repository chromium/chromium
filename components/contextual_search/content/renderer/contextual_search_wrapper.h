// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_RENDERER_CONTEXTUAL_SEARCH_WRAPPER_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_RENDERER_CONTEXTUAL_SEARCH_WRAPPER_H_

#include "base/macros.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "gin/handle.h"
#include "gin/wrappable.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace blink {
class WebFrame;
}

namespace contextual_search {

// Wrapper for injecting Contextual Search JavaScript
// into a WebFrame.
class ContextualSearchWrapper : public gin::Wrappable<ContextualSearchWrapper>,
                                public content::RenderFrameObserver {
 public:
  // Installs Contextual Search JavaScript.
  static void Install(content::RenderFrame* render_frame);

  // RenderFrameObserver implementation.
  void OnDestruct() override;

  // Required by gin::Wrappable.
  static gin::WrapperInfo kWrapperInfo;

 private:
  // Instantiate by calling Install.
  explicit ContextualSearchWrapper(content::RenderFrame* render_frame);
  ~ContextualSearchWrapper() override;

  // gin::Wrappable.
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  // Called by JavaScript to set the caption , and indicate whether
  // the caption provides an answer (e.g. an actual definition), rather than
  // just general notification of what kind of answer may be available.
  void SetCaption(const std::string& caption, bool does_answer);

  // Called by JavaScript to change the Overlay position.
  // The panel cannot be changed to any opened position if it's not already
  // opened.
  void ChangeOverlayPosition(unsigned int desired_position);

  // Helper function to ensure that this class has connected to the API service.
  // Returns false if cannot connect.
  bool EnsureServiceConnected();

  // The service to notify when API calls are made.
  mojo::Remote<mojom::ContextualSearchJsApiService>
      contextual_search_js_api_service_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchWrapper);
};

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_CONTENT_RENDERER_CONTEXTUAL_SEARCH_WRAPPER_H_
