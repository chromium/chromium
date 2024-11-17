// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/public/tab_group_sync_service.h"

namespace tab_groups {

CollaborationFinder* TabGroupSyncService::GetCollaborationFinderForTesting() {
  return nullptr;
}

}  // namespace tab_groups
