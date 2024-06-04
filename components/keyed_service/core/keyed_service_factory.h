// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_FACTORY_H_

#include "components/keyed_service/core/keyed_service.h"
#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_templated_factory.h"

// Base class for Factories that take an opaque pointer and return some service
// on a one-to-one mapping. Each concrete factory that derives from this class
// *must* be a Singleton (only unit tests don't do that).
//
// We do this because services depend on each other and we need to control
// shutdown/destruction order. In each derived classes' constructors, the
// implementors must explicitly state which services are depended on.
class KEYED_SERVICE_EXPORT KeyedServiceFactory
    : public KeyedServiceTemplatedFactory<KeyedService> {
 protected:
  using KeyedServiceTemplatedFactory<
      KeyedService>::KeyedServiceTemplatedFactory;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_FACTORY_H_
