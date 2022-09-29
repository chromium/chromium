// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/libassistant/public/cpp/libassistant_loader.h"

namespace ash::libassistant {

// static
void LibassistantLoader::Load(LoadCallback callback) {
  std::move(callback).Run(/*success=*/true);
}

}  // namespace ash::libassistant
