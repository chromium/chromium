// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/location_bar.h"

LocationBar::Observer::~Observer() = default;

void LocationBar::Observer::OnLocationBarBoundsChanged() {}

LocationBar::LocationBar(CommandUpdater* command_updater)
    : command_updater_(command_updater) {}

void LocationBar::AddLocationBarObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void LocationBar::RemoveLocationBarObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

LocationBar::~LocationBar() = default;

void LocationBar::NotifyBoundsChanged() {
  observers_.Notify(&Observer::OnLocationBarBoundsChanged);
}

bool LocationBar::in_popup_state_transition() const {
  return false;
}
