// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_receiver/browser/public/content_window_controls.h"

namespace cast_receiver {

ContentWindowControls::VisibilityChangeObserver::~VisibilityChangeObserver() =
    default;

ContentWindowControls::ContentWindowControls() = default;

ContentWindowControls::~ContentWindowControls() = default;

void ContentWindowControls::AddVisibilityChangeObserver(
    VisibilityChangeObserver& observer) {
  visibility_state_observer_list_.AddObserver(&observer);
}

void ContentWindowControls::RemoveVisibilityChangeObserver(
    VisibilityChangeObserver& observer) {
  visibility_state_observer_list_.RemoveObserver(&observer);
}

void ContentWindowControls::OnWindowShown() {
  for (auto& observer : visibility_state_observer_list_) {
    observer.OnWindowShown();
  }
}

void ContentWindowControls::OnWindowHidden() {
  for (auto& observer : visibility_state_observer_list_) {
    observer.OnWindowHidden();
  }
}

}  // namespace cast_receiver
