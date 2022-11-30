// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_input_delegate.h"

namespace exo {
namespace wayland {

WaylandInputDelegate::WaylandInputDelegate() = default;

void WaylandInputDelegate::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WaylandInputDelegate::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WaylandInputDelegate::SendTimestamp(base::TimeTicks time_stamp) {
  for (auto& observer : observers_)
    observer.OnSendTimestamp(time_stamp);
}

WaylandInputDelegate::~WaylandInputDelegate() {
  for (auto& observer : observers_)
    observer.OnDelegateDestroying(this);
}

}  // namespace wayland
}  // namespace exo
