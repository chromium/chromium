// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_input_watcher.h"
#include "base/observer_list.h"
#include "build/build_config.h"

OmniboxInputWatcher::OmniboxInputWatcher() = default;
OmniboxInputWatcher::~OmniboxInputWatcher() = default;

void OmniboxInputWatcher::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void OmniboxInputWatcher::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void OmniboxInputWatcher::NotifyInputEntered() {
  for (auto& observer : observers_)
    observer.OnOmniboxInputEntered();
}
