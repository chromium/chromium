// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_H_

#include "components/keyed_service/core/keyed_service_export.h"

// Interface for keyed services that support two-phase destruction order.
//
// Two-phase shutdown allows keyed services to have a first pass shutdown phase
// where they drop references. Not all services will need this, so there's a
// default implementation. Only once every service has been given a chance to
// drop references are services deleted. In a service's destructor the service
// should *not* request other services from their factories via the relevant
// Context object (e.g., Profile), as the association between that Context
// object and its keyed services is dropped after the shutdown phase.
// Shutdown of KeyedServices is generally initiated by the embedder's
// destruction of Profile (or analogous object).
// CAVEAT: Not all embedders destroy the Profiles (or Profile analogs) as part
// of embedder shutdown, so it is not guaranteed that the keyed service shutdown
// process will run at shutdown of a given embedder.
class KEYED_SERVICE_EXPORT KeyedService {
 public:
  KeyedService() = default;

  KeyedService(const KeyedService&) = delete;
  KeyedService& operator=(const KeyedService&) = delete;
  KeyedService(KeyedService&&) = delete;
  KeyedService& operator=(KeyedService&&) = delete;

  // The second pass is the actual deletion of each object.
  virtual ~KeyedService() = default;

  // The first pass is to call Shutdown on a KeyedService.
  // Shutdown will be called automatically for you. Don't directly invoke
  // this unless you have a specific reason and understand the implications.
  virtual void Shutdown() {}
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_H_
