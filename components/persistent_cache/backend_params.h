// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_H_
#define COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_H_

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/files/file.h"

namespace persistent_cache {

enum class BackendType {
  kMock = 0,
  kMaxValue = kMock,
};

// This struct contains fields necessary to configure a persistent
// cache backend.
struct COMPONENT_EXPORT(PERSISTENT_CACHE) BackendParams {
  BackendParams();
  ~BackendParams();

  BackendType type;
  base::flat_map<std::string, base::File> files;
};

}  // namespace persistent_cache

#endif  // COMPONENTS_PERSISTENT_CACHE_BACKEND_PARAMS_H_
