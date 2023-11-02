// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLABILITY_AGENT_H_
#define COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLABILITY_AGENT_H_

#include "content/public/renderer/render_frame_observer.h"

namespace dom_distiller {

// DistillabilityAgent returns distillability result to DistillabilityDriver.
class DistillabilityAgent : public content::RenderFrameObserver {
 public:
  DistillabilityAgent(content::RenderFrame* render_frame, bool dump_info);
  ~DistillabilityAgent() override;

  // content::RenderFrameObserver:
  void DidMeaningfulLayout(blink::WebMeaningfulLayout layout_type) override;
  void OnDestruct() override;

 private:
  bool dump_info_;
};

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CONTENT_RENDERER_DISTILLABILITY_AGENT_H_
