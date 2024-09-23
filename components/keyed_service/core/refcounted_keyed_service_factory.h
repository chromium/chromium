// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_FACTORY_H_
#define COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_FACTORY_H_

#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_templated_factory.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"

// A specialized KeyedServiceBaseFactory that manages a RefcountedThreadSafe<>.
//
// While the factory returns RefcountedThreadSafe<>s, the factory itself is a
// base::NotThreadSafe. Only call methods on this object on the UI thread.
//
// Implementers of RefcountedKeyedService should note that we guarantee that
// ShutdownOnUIThread() is called on the UI thread, but actual object
// destruction can happen anywhere.
class KEYED_SERVICE_EXPORT RefcountedKeyedServiceFactory
    : public KeyedServiceTemplatedFactory<RefcountedKeyedService> {
 public:
  using KeyedServiceTemplatedFactory<
      RefcountedKeyedService>::KeyedServiceTemplatedFactory;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_REFCOUNTED_KEYED_SERVICE_FACTORY_H_
