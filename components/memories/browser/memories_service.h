// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEMORIES_BROWSER_MEMORIES_SERVICE_H_
#define COMPONENTS_MEMORIES_BROWSER_MEMORIES_SERVICE_H_

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace memories {

// This Service is the API for UIs to fetch Chrome Memories.
class MemoriesService : public KeyedService {
 public:
  MemoriesService() = default;
  ~MemoriesService() override;

  // KeyedService:
  void Shutdown() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MemoriesService);
};

}  // namespace memories

#endif  // COMPONENTS_MEMORIES_BROWSER_MEMORIES_SERVICE_H_
