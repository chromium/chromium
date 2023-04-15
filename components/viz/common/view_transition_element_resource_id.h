// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_
#define COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// See share_element_resource_id.mojom for details.
class VIZ_COMMON_EXPORT ViewTransitionElementResourceId {
 public:
  // Generates a new id.
  static ViewTransitionElementResourceId Generate();

  // For mojo deserialization.
  explicit ViewTransitionElementResourceId(uint32_t id);

  // Creates an invalid id.
  ViewTransitionElementResourceId();
  ~ViewTransitionElementResourceId();

  bool operator==(const ViewTransitionElementResourceId& o) const {
    return id_ == o.id_;
  }
  bool operator!=(const ViewTransitionElementResourceId& o) const {
    return !(*this == o);
  }
  bool operator<(const ViewTransitionElementResourceId& o) const {
    return id_ < o.id_;
  }

  bool IsValid() const;
  std::string ToString() const;

  uint32_t id() const { return id_; }

 private:
  uint32_t id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_VIEW_TRANSITION_ELEMENT_RESOURCE_ID_H_
