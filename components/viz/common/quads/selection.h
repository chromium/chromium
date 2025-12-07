// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_SELECTION_H_
#define COMPONENTS_VIZ_COMMON_QUADS_SELECTION_H_

namespace viz {

template <typename BoundType>
struct Selection {
  Selection() = default;
  ~Selection() = default;

  BoundType start;
  BoundType end;

  friend bool operator==(const Selection<BoundType>&,
                         const Selection<BoundType>&) = default;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_SELECTION_H_
