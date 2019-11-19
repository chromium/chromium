// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/viz_utils.h"

#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/rrect_f.h"

namespace viz {

bool PreferRGB565ResourcesForDisplay() {
#if defined(OS_ANDROID)
  return base::SysInfo::AmountOfPhysicalMemoryMB() <= 512;
#endif
  return false;
}

bool GetScaledRegion(const gfx::Rect& rect,
                     const gfx::QuadF* clip,
                     gfx::QuadF* scaled_region) {
  if (!clip)
    return false;

  gfx::PointF p1(((clip->p1().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p1().y() - rect.y()) / rect.height()) - 0.5f);
  gfx::PointF p2(((clip->p2().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p2().y() - rect.y()) / rect.height()) - 0.5f);
  gfx::PointF p3(((clip->p3().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p3().y() - rect.y()) / rect.height()) - 0.5f);
  gfx::PointF p4(((clip->p4().x() - rect.x()) / rect.width()) - 0.5f,
                 ((clip->p4().y() - rect.y()) / rect.height()) - 0.5f);
  *scaled_region = gfx::QuadF(p1, p2, p3, p4);
  return true;
}

bool GetScaledRRectF(const gfx::Rect& space,
                     const gfx::RRectF& rect,
                     gfx::RRectF* scaled_rect) {
  float x_scale = 1.0f / space.width();
  float y_scale = 1.0f / space.height();
  float new_x = (rect.rect().x() - space.x()) * x_scale - 0.5f;
  float new_y = (rect.rect().y() - space.y()) * y_scale - 0.5f;
  *scaled_rect = rect;
  scaled_rect->Scale(x_scale, y_scale);
  scaled_rect->Offset(-scaled_rect->rect().origin().x(),
                      -scaled_rect->rect().origin().y());
  scaled_rect->Offset(new_x, new_y);
  return true;
}

bool GetScaledUVs(const gfx::Rect& rect, const gfx::QuadF* clip, float uvs[8]) {
  if (!clip)
    return false;

  uvs[0] = ((clip->p1().x() - rect.x()) / rect.width());
  uvs[1] = ((clip->p1().y() - rect.y()) / rect.height());
  uvs[2] = ((clip->p2().x() - rect.x()) / rect.width());
  uvs[3] = ((clip->p2().y() - rect.y()) / rect.height());
  uvs[4] = ((clip->p3().x() - rect.x()) / rect.width());
  uvs[5] = ((clip->p3().y() - rect.y()) / rect.height());
  uvs[6] = ((clip->p4().x() - rect.x()) / rect.width());
  uvs[7] = ((clip->p4().y() - rect.y()) / rect.height());
  return true;
}

}  // namespace viz
