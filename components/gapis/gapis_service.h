// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GAPIS_GAPIS_SERVICE_H_
#define COMPONENTS_GAPIS_GAPIS_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace gapis {

class GapisService : public KeyedService {
 public:
  GapisService() = default;
  ~GapisService() override = default;
};

}  // namespace gapis

#endif  // COMPONENTS_GAPIS_GAPIS_SERVICE_H_
