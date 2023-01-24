// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update_forward.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "base/memory/weak_ptr.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

namespace content {
class RenderFrame;
}

namespace ui {
class AXTree;
}

///////////////////////////////////////////////////////////////////////////////
// AXTreeDistiller
//
//  A class that distills an AXTreeUpdate. The main API is
//  AXTreeDistiller::Distill(), which kicks off the distillation. Once a
//  distilled AXTree is ready, calls a callback.
//  When |IsReadAnythingWithScreen2xEnabled()|, the distillation is performed
//  by the Screen2x ML model in the utility process. Otherwise, distillation is
//  done using rules defined in this file.
//
class AXTreeDistiller {
  using OnAXTreeDistilledCallback = base::RepeatingCallback<void(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXNodeID>& content_node_ids)>;

 public:
  AXTreeDistiller(content::RenderFrame* render_frame,
                  OnAXTreeDistilledCallback on_ax_tree_distilled_callback);
  virtual ~AXTreeDistiller();
  AXTreeDistiller(const AXTreeDistiller&) = delete;
  AXTreeDistiller& operator=(const AXTreeDistiller&) = delete;

  // Distills the AXTree. |tree| and |snapshot| are duplicates of each other;
  // the algorithm requires the data in |tree| form while Screen2x requires it
  // in |snapshot| form.
  // When |IsReadAnythingWithScreen2xEnabled|, this operation is done in the
  // utility process by Screen2x. Otherwise, it is done by a rules-based
  // algorithm in this process.
  virtual void Distill(const ui::AXTree& tree,
                       const ui::AXTreeUpdate& snapshot);

 private:
  // Distills the AXTree via a rules-based algorithm. Runs the callback on
  // completion.
  void DistillViaAlgorithm(const ui::AXTree& tree);

  // render_frame_ is only used in the ENABLE_SCREEN_AI_SERVICE buildflag.
  // Fuchsia does not build with that buildflag so it is throwing
  // -Wunused-private-field errors. [[maybe_unused]] suppresses them.
  [[maybe_unused]] content::RenderFrame* render_frame_;

  // TODO(crbug.com/1266555): Ensure this is called even if ScreenAIService is
  // disconnected.
  OnAXTreeDistilledCallback on_ax_tree_distilled_callback_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Passes |snapshot| to the Screen2x ML model, which identifes the main
  // content nodes and calls |ProcessScreen2xResult()| on completion.
  void DistillViaScreen2x(const ui::AXTree& tree,
                          const ui::AXTreeUpdate& snapshot);

  // Called by the Screen2x service from the utility process. Runs the callback
  // if Screen2x identified content nodes. If not, distills via the rules-based
  // algorithm.
  void ProcessScreen2xResult(const ui::AXTree& tree,
                             const std::vector<ui::AXNodeID>& content_node_ids);

  // Called when the main content extractor is disconnected. Runs the callback
  // with an empty list of content node IDs.
  void OnMainContentExtractorDisconnected();

  // The remote of the Screen2x main content extractor. The receiver lives in
  // the utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;
  base::WeakPtrFactory<AXTreeDistiller> weak_ptr_factory_{this};
#endif
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
