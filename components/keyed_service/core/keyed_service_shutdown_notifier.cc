// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/keyed_service/core/keyed_service_export.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"

KeyedServiceShutdownNotifier::KeyedServiceShutdownNotifier() {
}
KeyedServiceShutdownNotifier::~KeyedServiceShutdownNotifier() {
}

base::CallbackListSubscription KeyedServiceShutdownNotifier::Subscribe(
    const base::RepeatingClosure& callback) {
  return callback_list_.Add(callback);
}

void KeyedServiceShutdownNotifier::Shutdown() {
  callback_list_.Notify();
}
