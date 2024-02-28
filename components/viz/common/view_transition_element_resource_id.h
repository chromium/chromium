// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_
#define COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_

#include <stdint.h>

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include "base/unguessable_token.h"
#include "components/viz/common/viz_common_export.h"

namespace viz {

using TransitionId = base::UnguessableToken;

// See view_transition_element_resource_id.mojom for details.
class VIZ_COMMON_EXPORT ViewTransitionElementResourceId {
 public:
  static constexpr uint32_t kInvalidLocalId = 0;

  ViewTransitionElementResourceId(const TransitionId& transition_id,
                                  uint32_t local_id);

  // Creates an invalid id.
  ViewTransitionElementResourceId();
  ~ViewTransitionElementResourceId();

  friend bool operator==(const ViewTransitionElementResourceId&,
                         const ViewTransitionElementResourceId&) = default;
  friend auto operator<=>(const ViewTransitionElementResourceId&,
                          const ViewTransitionElementResourceId&) = default;

  bool IsValid() const;
  std::string ToString() const;

  uint32_t local_id() const { return local_id_; }
  const TransitionId& transition_id() const { return transition_id_; }

 private:
  // Refers to a specific view transition - globally unique.
  TransitionId transition_id_;

  // Refers to a specific snapshot resource within a specific transition
  // Unique only with respect to a given `transition_id_`.
  uint32_t local_id_ = kInvalidLocalId;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_
