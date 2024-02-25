// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/update_did_run_state.h"

#include <windows.h>

#include "base/win/registry.h"
#include "chrome/install_static/install_util.h"
#include "chrome/installer/util/google_update_constants.h"

namespace installer {

void UpdateDidRunState() {
  base::win::RegKey key;
  if (key.Create(HKEY_CURRENT_USER,
                 install_static::GetClientStateKeyPath().c_str(),
                 KEY_SET_VALUE | KEY_WOW64_32KEY) == ERROR_SUCCESS) {
    key.WriteValue(google_update::kRegDidRunField, L"1");
  }
}

}  // namespace installer
