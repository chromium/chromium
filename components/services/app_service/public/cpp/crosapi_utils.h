// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CROSAPI_UTILS_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CROSAPI_UTILS_H_

// Utility functions for App Service crosapi usage.

#include <memory>
#include <vector>

namespace apps_util {

// Clone a list of StructPtr.
template <typename StructPtr>
std::vector<StructPtr> CloneStructPtrVector(
    const std::vector<StructPtr>& clone_from) {
  std::vector<StructPtr> clone_to;
  for (const auto& struct_ptr : clone_from) {
    clone_to.push_back(struct_ptr->Clone());
  }
  return clone_to;
}

}  // namespace apps_util

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_CROSAPI_UTILS_H_
