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

#include "components/viz/common/viz_common_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace viz {

// See view_transition_element_resource_id.mojom for details.
class VIZ_COMMON_EXPORT ViewTransitionElementResourceId {
 public:
  static constexpr uint32_t kInvalidLocalId = 0;

  ViewTransitionElementResourceId(
      const blink::ViewTransitionToken& transition_token,
      uint32_t local_id);

  // Creates an invalid id.
  ViewTransitionElementResourceId();
  ~ViewTransitionElementResourceId();

  VIZ_COMMON_EXPORT friend bool operator==(
      const ViewTransitionElementResourceId&,
      const ViewTransitionElementResourceId&);
  friend auto operator<=>(const ViewTransitionElementResourceId&,
                          const ViewTransitionElementResourceId&) = default;

  bool IsValid() const;
  std::string ToString() const;

  uint32_t local_id() const { return local_id_; }
  const blink::ViewTransitionToken& transition_token() const {
    CHECK(transition_token_);
    return *transition_token_;
  }

 private:
  // Refers to a specific view transition - globally unique.
  std::optional<blink::ViewTransitionToken> transition_token_;

  // Refers to a specific snapshot resource within a specific transition
  // Unique only with respect to a given `transition_token_`.
  uint32_t local_id_ = kInvalidLocalId;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_
