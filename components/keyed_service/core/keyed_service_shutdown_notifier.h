// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_SHUTDOWN_NOTIFIER_H_
#define COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_SHUTDOWN_NOTIFIER_H_

#include "base/callback_list.h"
#include "components/keyed_service/core/keyed_service.h"

// This is a helper class for objects that depend on one or more keyed services,
// but which cannot be keyed services themselves, for example because they don't
// correspond 1:1 to a context, or because they have a different lifetime.
//
// To use this class, add a factory class and declare the dependencies there.
// This class (being a KeyedService itself) will be shut down before its
// dependencies and notify its observers.
class KEYED_SERVICE_EXPORT KeyedServiceShutdownNotifier : public KeyedService {
 public:
  KeyedServiceShutdownNotifier();

  KeyedServiceShutdownNotifier(const KeyedServiceShutdownNotifier&) = delete;
  KeyedServiceShutdownNotifier& operator=(const KeyedServiceShutdownNotifier&) =
      delete;

  ~KeyedServiceShutdownNotifier() override;

  // Subscribe for a notification when the keyed services this object depends on
  // (as defined by its factory) are shut down. The subscription can be
  // destroyed to unsubscribe.
  base::CallbackListSubscription Subscribe(base::OnceClosure callback);

 private:
  // KeyedService implementation:
  void Shutdown() override;

  base::OnceClosureList closure_list_;
};

#endif  // COMPONENTS_KEYED_SERVICE_CORE_KEYED_SERVICE_SHUTDOWN_NOTIFIER_H_
