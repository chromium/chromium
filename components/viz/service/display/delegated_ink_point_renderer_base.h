// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_BASE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_BASE_H_

#include <memory>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "components/viz/common/quads/aggregated_render_pass.h"
#include "components/viz/service/display/delegated_ink_trail_data.h"
#include "components/viz/service/viz_service_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/gfx/delegated_ink_metadata.h"
#include "ui/gfx/mojom/delegated_ink_point_renderer.mojom.h"

namespace gfx {
class DelegatedInkPoint;
}  // namespace gfx

namespace viz {

// This is the base class used for rendering delegated ink trails on the end of
// strokes to reduce user perceived latency. On initialization, it binds the
// mojo interface required for receiving delegated ink points that are made and
// sent from the browser process.
//
// For more information on the feature, please see the explainer:
// https://github.com/WICG/ink-enhancement/blob/main/README.md
class VIZ_SERVICE_EXPORT DelegatedInkPointRendererBase
    : public gfx::mojom::DelegatedInkPointRenderer {
 public:
  DelegatedInkPointRendererBase();
  ~DelegatedInkPointRendererBase() override;
  DelegatedInkPointRendererBase(const DelegatedInkPointRendererBase&) = delete;
  DelegatedInkPointRendererBase& operator=(
      const DelegatedInkPointRendererBase&) = delete;

  void InitMessagePipeline(
      mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer> receiver);

  void StoreDelegatedInkPoint(const gfx::DelegatedInkPoint& point) override;
  virtual void SetDelegatedInkMetadata(
      std::unique_ptr<gfx::DelegatedInkMetadata> metadata);

  virtual void FinalizePathForDraw() = 0;
  virtual gfx::Rect GetDamageRect() = 0;

  // This function is called after Delegated Ink's points are submitted to be
  // drawn on screen, and fires a histogram with the time between points' event
  // creation and the points' draw submission to the OS.
  void ReportPointsDrawn();

  // Get the of the render pass that the Delegated Ink trail should be drawn on.
  // This id is initially set on the metadata during surface aggregation.
  std::optional<AggregatedRenderPassId> GetLatestMetadataRenderPassId() const;

 protected:
  // `pointer_ids_` is not emptied each time after the points are drawn, because
  // one point in `pointer_ids_` could potentially be drawn in more than one
  // delegated ink trail. However, if a point has a timestamp that is earlier
  // than the timestamp on the metadata, then the point has already been drawn,
  // and therefore should be removed from `pointer_ids_` before drawing.
  std::vector<gfx::DelegatedInkPoint> FilterPoints();

  // Empties `pointer_ids_` and resets the pointer_id_` if there is no
  // `metadata_` when `FinalizePathForDraw()` gets called.
  void ResetPoints();

  void PredictPoints(std::vector<gfx::DelegatedInkPoint>* ink_points_to_draw);
  void ResetPrediction() override;

  std::unique_ptr<gfx::DelegatedInkMetadata> metadata_;

 private:
  friend class DelegatedInkDisplayTest;
  friend class SkiaDelegatedInkRendererTest;

  const std::unordered_map<int32_t, DelegatedInkTrailData>&
  GetPointsMapForTest() const {
    return pointer_ids_;
  }

  const gfx::DelegatedInkMetadata* GetMetadataForTest() const {
    return metadata_.get();
  }

  virtual int GetPathPointCountForTest() const = 0;

  // Cached pointer id that matches the most recent metadata. This is set when
  // a metadata arrives, and if no stored DelegatedInkPoints match the metadata,
  // then it is null.
  std::optional<int32_t> pointer_id_;

  // The points that arrived from the browser process and may be drawn as part
  // of the ink trail are stored according to their pointer ids so that if
  // more than one source of points is arriving, we can choose the correct set
  // of points to use when drawing the delegated ink trail.
  std::unordered_map<int32_t, DelegatedInkTrailData> pointer_ids_;

  mojo::Receiver<gfx::mojom::DelegatedInkPointRenderer> receiver_{this};

  // The timestamp in which a Delegated Ink point that matches the metadata was
  // first painted.
  std::optional<base::TimeTicks> metadata_paint_time_;

  // `metadata_` gets deleted and re-set on every paint cycle. This variable
  // persists the metadata value to avoid incorrect repetitions of histogram
  // fires for the same metadata.
  std::optional<gfx::DelegatedInkMetadata> previous_metadata_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_RENDERER_BASE_H_
