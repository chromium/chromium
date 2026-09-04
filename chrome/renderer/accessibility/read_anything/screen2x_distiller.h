// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_SCREEN2X_DISTILLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_SCREEN2X_DISTILLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_distiller.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"

namespace content {
class RenderFrame;
}  // namespace content

class AXTreeDistiller;

// Concrete implementation of ReadAnythingDistiller for Screen2x and rules-based
// accessibility tree distillation.
class Screen2xDistiller : public ReadAnythingDistiller {
 public:
  using ScreenAIReadinessCallback = base::RepeatingCallback<bool()>;

  Screen2xDistiller(
      content::RenderFrame* render_frame,
      ScreenAIReadinessCallback is_screen_ai_ready_callback,
      DistillationCompleteCallback on_distillation_complete_callback);
  ~Screen2xDistiller() override;
  Screen2xDistiller(const Screen2xDistiller&) = delete;
  Screen2xDistiller& operator=(const Screen2xDistiller&) = delete;

  // ReadAnythingDistiller:
  void Distill(const DistillationRequest& request) override;
  void Reset() override;
  bool IsInProgress() const override;

 private:
  void OnAXTreeDistilled(const ui::AXTreeID& tree_id,
                         const std::vector<ui::AXNodeID>& content_node_ids);

  ScreenAIReadinessCallback is_screen_ai_ready_callback_;
  DistillationCompleteCallback on_distillation_complete_callback_;
  std::unique_ptr<AXTreeDistiller> distiller_;
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_SCREEN2X_DISTILLER_H_
