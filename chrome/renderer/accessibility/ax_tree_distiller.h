// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame_observer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace content {
class RenderFrame;
}

namespace ui {
class AXTree;
}

namespace ukm {
class MojoUkmRecorder;
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
class AXTreeDistiller : public content::RenderFrameObserver {
  using OnAXTreeDistilledCallback = base::RepeatingCallback<void(
      const ui::AXTreeID& tree_id,
      const std::vector<ui::AXNodeID>& content_node_ids)>;

 public:
  explicit AXTreeDistiller(
      content::RenderFrame* render_frame,
      OnAXTreeDistilledCallback on_ax_tree_distilled_callback);
  ~AXTreeDistiller() override;
  AXTreeDistiller(const AXTreeDistiller&) = delete;
  AXTreeDistiller& operator=(const AXTreeDistiller&) = delete;

  // Distills the AXTree. |tree| and |snapshot| are duplicates of each other;
  // the algorithm requires the data in |tree| form while Screen2x requires it
  // in |snapshot| form.
  // When |IsReadAnythingWithScreen2xEnabled|, this operation is done in the
  // utility process by Screen2x. Otherwise, it is done by a rules-based
  // algorithm in this process.
  virtual void Distill(const ui::AXTree& tree,
                       const ui::AXTreeUpdate& snapshot,
                       const ukm::SourceId ukm_source_id);

  void ScreenAIServiceReady();

  // content::RenderFrameObserver:
  void OnDestruct() override {}

 private:
  // Distills the AXTree via a rules-based algorithm. Results are added to
  // |content_node_ids|.
  void DistillViaAlgorithm(const ui::AXTree& tree,
                           const ukm::SourceId ukm_source_id,
                           std::vector<ui::AXNodeID>* content_node_ids);

  void RecordRulesMetrics(const ukm::SourceId ukm_source_id,
                          base::TimeDelta elapsed_time,
                          bool success);

  // Passes |snapshot| to the Screen2x ML model, which identifes the main
  // content nodes and calls |ProcessScreen2xResult()| on completion.
  // |content_node_ids_algorithm| are the content nodes identified by the
  // algorithm. They are passed along to the screen2x callback. start_time is
  // the time when DistillViaAlgorithm started and is used for
  // RecordMergedMetrics.
  void DistillViaScreen2x(
      const ui::AXTree& tree,
      const ui::AXTreeUpdate& snapshot,
      const ukm::SourceId ukm_source_id,
      base::TimeTicks start_time,
      std::vector<ui::AXNodeID>* content_node_ids_algorithm);

  // Called by the Screen2x service from the utility process. Merges the result
  // from the algorithm with the result from Screen2x and passes the merged
  // vector to the callback.
  void ProcessScreen2xResult(
      const ui::AXTreeID& tree_id,
      const ukm::SourceId ukm_source_id,
      base::TimeTicks start_time,
      std::vector<ui::AXNodeID> content_node_ids_algorithm,
      const std::vector<ui::AXNodeID>& content_node_ids_screen2x);

  // Called when the main content extractor is disconnected. Runs the callback
  // with an empty list of content node IDs.
  void OnMainContentExtractorDisconnected();

  // Record time it takes for the merged algorithm (distillation via algorithm
  // and via Screen2x) to run.
  void RecordMergedMetrics(const ukm::SourceId ukm_source_id,
                           base::TimeDelta elapsed_time,
                           bool success);

  // TODO(crbug.com/40802192): Ensure this is called even if ScreenAIService is
  // disconnected.
  OnAXTreeDistilledCallback on_ax_tree_distilled_callback_;

  std::unique_ptr<ukm::MojoUkmRecorder> ukm_recorder_;

  // ScreenAI service is successfully initialized.
  bool screen_ai_service_ready_ = false;

  // The remote of the Screen2x main content extractor. The receiver lives in
  // the utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;
  base::WeakPtrFactory<AXTreeDistiller> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
