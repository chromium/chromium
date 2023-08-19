// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/lacros_startup_state.h"

namespace {

bool g_is_lacros_enabled = false;

}  //  namespace

namespace crosapi {

namespace lacros_startup_state {

void SetLacrosStartupState(bool is_enabled) {
  g_is_lacros_enabled = is_enabled;
}

bool IsLacrosEnabled() {
  return g_is_lacros_enabled;
}

}  // namespace lacros_startup_state

}  //  namespace crosapi
