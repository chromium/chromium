// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/libassistant/public/cpp/libassistant_loader.h"

namespace chromeos::libassistant {

// static
void LibassistantLoader::Load(LoadCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

}  // namespace chromeos::libassistant
