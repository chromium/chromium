// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_BASE_PAGE_TRANSITION_CONVERSION_H_
#define COMPONENTS_SYNC_BASE_PAGE_TRANSITION_CONVERSION_H_

#include "ui/base/page_transition_types.h"

namespace sync_pb {
enum SyncEnums_PageTransition : int;
}  // namespace sync_pb

namespace syncer {

sync_pb::SyncEnums_PageTransition ToSyncPageTransition(
    ui::PageTransition transition_type);

ui::PageTransition FromSyncPageTransition(
    sync_pb::SyncEnums_PageTransition transition_type);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_PAGE_TRANSITION_CONVERSION_H_
