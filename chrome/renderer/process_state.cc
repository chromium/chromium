// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/process_state.h"

namespace {

bool g_is_incognito_process = false;

}  // namespace

bool IsIncognitoProcess() {
  return g_is_incognito_process;
}

void SetIsIncognitoProcess(bool is_incognito_process) {
  g_is_incognito_process = is_incognito_process;
}
