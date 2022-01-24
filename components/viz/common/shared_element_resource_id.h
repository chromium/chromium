// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SHARED_ELEMENT_RESOURCE_ID_H_
#define COMPONENTS_VIZ_COMMON_SHARED_ELEMENT_RESOURCE_ID_H_

#include <string>
#include <vector>

#include "components/viz/common/viz_common_export.h"

namespace viz {

// See share_element_resource_id.mojom for details.
class VIZ_COMMON_EXPORT SharedElementResourceId {
 public:
  // Generates a new id.
  static SharedElementResourceId Generate();

  // For mojo deserialization.
  explicit SharedElementResourceId(uint32_t id);

  // Creates an invalid id.
  SharedElementResourceId();
  ~SharedElementResourceId();

  bool operator==(const SharedElementResourceId& o) const {
    return id_ == o.id_;
  }
  bool operator!=(const SharedElementResourceId& o) const {
    return !(*this == o);
  }
  bool operator<(const SharedElementResourceId& o) const { return id_ < o.id_; }

  bool IsValid() const;
  std::string ToString() const;

  uint32_t id() const { return id_; }

 private:
  uint32_t id_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SHARED_ELEMENT_RESOURCE_ID_H_
