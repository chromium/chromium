// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/input_event.h"

namespace vr {

InputEvent::InputEvent(Type type) : type_(type) {}

InputEvent::~InputEvent() = default;

}  // namespace vr
