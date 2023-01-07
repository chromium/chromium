// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/initialization_task.h"
#include "base/observer_list.h"

namespace payments {

InitializationTask::Observer::~Observer() = default;

InitializationTask::InitializationTask() = default;

InitializationTask::~InitializationTask() = default;

void InitializationTask::AddInitializationObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void InitializationTask::RemoveInitializationObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void InitializationTask::NotifyInitialized() {
  DCHECK(!has_notified_);
  has_notified_ = true;
  for (Observer& observer : observers_) {
    observer.OnInitialized(this);
  }
}

}  // namespace payments
