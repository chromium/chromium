// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_QUAD_LIST_H_
#define COMPONENTS_VIZ_COMMON_QUADS_QUAD_LIST_H_

#include <stddef.h>

#include "cc/base/list_container.h"
#include "components/viz/common/quads/draw_quad.h"

namespace viz {

// A list of DrawQuad objects, sorted internally in front-to-back order. To
// add a new quad drawn behind another quad, it must be placed after the other
// quad.
class VIZ_COMMON_EXPORT QuadList : public cc::ListContainer<DrawQuad> {
 public:
  QuadList();
  explicit QuadList(size_t default_size_to_reserve);

  using BackToFrontIterator = QuadList::ReverseIterator;
  using ConstBackToFrontIterator = QuadList::ConstReverseIterator;

  inline BackToFrontIterator BackToFrontBegin() { return rbegin(); }
  inline BackToFrontIterator BackToFrontEnd() { return rend(); }
  inline ConstBackToFrontIterator BackToFrontBegin() const { return rbegin(); }
  inline ConstBackToFrontIterator BackToFrontEnd() const { return rend(); }

  Iterator InsertCopyBeforeDrawQuad(Iterator at, size_t count);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_QUAD_LIST_H_
