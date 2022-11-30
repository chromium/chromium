// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

KeyedServiceShutdownNotifier::KeyedServiceShutdownNotifier() = default;
KeyedServiceShutdownNotifier::~KeyedServiceShutdownNotifier() = default;

base::CallbackListSubscription KeyedServiceShutdownNotifier::Subscribe(
    base::OnceClosure callback) {
  return closure_list_.Add(std::move(callback));
}

void KeyedServiceShutdownNotifier::Shutdown() {
  closure_list_.Notify();
}
