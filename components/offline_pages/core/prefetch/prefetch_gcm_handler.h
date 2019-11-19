// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_HANDLER_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_HANDLER_H_

#include <string>

#include "components/gcm_driver/instance_id/instance_id.h"

namespace gcm {
class GCMAppHandler;
}  // namespace gcm

namespace offline_pages {

class PrefetchService;

// Provides a GCM interface for PrefetchService.
class PrefetchGCMHandler {
 public:
  PrefetchGCMHandler() = default;
  virtual ~PrefetchGCMHandler() = default;

  // Sets the prefetch service. Must be called before using the handler.
  virtual void SetService(PrefetchService* service) = 0;

  // Returns the GCMAppHandler for this object.  Can return |nullptr| in unit
  // tests.
  virtual gcm::GCMAppHandler* AsGCMAppHandler() = 0;

  // The app ID to register with at the GCM layer.
  virtual std::string GetAppId() const = 0;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_PREFETCH_GCM_HANDLER_H_
