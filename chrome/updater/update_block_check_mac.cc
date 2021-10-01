// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_block_check.h"

#include <utility>

#include "base/callback.h"
#include "chrome/updater/update_service.h"

namespace updater {

// Blocking on metered network is not supported because macOS does not offer a
// way to detect if a connection is metered.
void ShouldBlockUpdateForMeteredNetwork(
    UpdateService::Priority,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}

}  // namespace updater
