// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_PIXEL_TEST_HELPER_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_PIXEL_TEST_HELPER_H_

#include <unordered_map>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/gfx/delegated_ink_metadata.h"

namespace gfx {
class DelegatedInkPoint;
}  // namespace gfx

namespace viz {

class DelegatedInkPointRendererBase;
class DirectRenderer;

// Helper class for running pixel tests to flex the delegated ink point
// renderers. |renderer_| must be supplied, either when constructed or later
// after cc::PixelTest::SetUp() has run, which creates the renderer. Then it
// can be used to create and give ink metadata and points to |renderer_|'s
// delegated ink point renderer. The metadata and points are stored so that the
// information can be used for future points and metadata (i.e. when a point
// needs to match the metadata for the first point of a trail) or when
// determining the damage rect to apply to a render pass.
class DelegatedInkPointPixelTestHelper {
 public:
  DelegatedInkPointPixelTestHelper();
  ~DelegatedInkPointPixelTestHelper();

  explicit DelegatedInkPointPixelTestHelper(DirectRenderer* renderer);
  void SetRendererAndCreateInkRenderer(DirectRenderer* renderer);
  void DropRenderer();

  void CreateAndSendMetadata(const gfx::PointF& point,
                             float diameter,
                             SkColor4f color,
                             base::TimeTicks timestamp,
                             const gfx::RectF& presentation_area,
                             const std::uint64_t render_pass_id);

  void CreateAndSendMetadataFromLastPoint();
  void CreateAndSendMetadataFromLastPoint(int32_t pointer_id);

  void CreateAndSendPoint(const gfx::PointF& point, base::TimeTicks timestamp);
  void CreateAndSendPoint(const gfx::PointF& point,
                          base::TimeTicks timestamp,
                          int32_t pointer_id);

  // Used when sending multiple points to be drawn as a single trail, so it uses
  // the most recently provided point's timestamp to determine the new one.
  void CreateAndSendPointFromLastPoint(const gfx::PointF& point);
  void CreateAndSendPointFromLastPoint(int32_t pointer_id,
                                       const gfx::PointF& point);

  gfx::Rect GetDelegatedInkDamageRect();
  gfx::Rect GetDelegatedInkDamageRect(int32_t pointer_id);

  const gfx::DelegatedInkMetadata& metadata() { return metadata_; }

 private:
  void CreateInkRenderer();

  raw_ptr<DirectRenderer> renderer_ = nullptr;
  raw_ptr<DelegatedInkPointRendererBase> ink_renderer_ = nullptr;
  std::unordered_map<int32_t, std::vector<gfx::DelegatedInkPoint>> ink_points_;
  gfx::DelegatedInkMetadata metadata_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_DELEGATED_INK_POINT_PIXEL_TEST_HELPER_H_
