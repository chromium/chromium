// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/notimplemented.h"
#include "components/system_media_controls/system_media_controls.h"

namespace system_media_controls {

// static
std::unique_ptr<SystemMediaControls> SystemMediaControls::Create(
    const std::string& product_name,
    int window) {
  return nullptr;
}

// static
void SetVisibilityChangedCallbackForTesting(
    base::RepeatingCallback<void(bool)>*) {
  NOTIMPLEMENTED();
}

}  // namespace system_media_controls
