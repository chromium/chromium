// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/global_media_controls/public/test/mock_media_item_manager.h"

namespace global_media_controls {
namespace test {

MockMediaItemManager::MockMediaItemManager() = default;

MockMediaItemManager::~MockMediaItemManager() = default;

base::WeakPtr<MediaItemManager> MockMediaItemManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace test
}  // namespace global_media_controls
