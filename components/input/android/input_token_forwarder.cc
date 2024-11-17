// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/android/input_token_forwarder.h"

#include "base/check.h"

namespace input {
namespace {
InputTokenForwarder* g_instance = nullptr;
}  // namespace

// static
InputTokenForwarder* InputTokenForwarder::GetInstance() {
  DCHECK(g_instance);
  return g_instance;
}

// static
void InputTokenForwarder::SetInstance(InputTokenForwarder* instance) {
  DCHECK(!g_instance || !instance);
  g_instance = instance;
}

}  // namespace input
