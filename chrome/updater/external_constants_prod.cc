// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/updater/external_constants.h"
#include "chrome/updater/external_constants_default.h"

namespace updater {

std::unique_ptr<ExternalConstants> CreateExternalConstants() {
  return CreateDefaultExternalConstants();
}

}  // namespace updater
