// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_CONTAINER_ID_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_CONTAINER_ID_H_

#include "base/types/strong_alias.h"
#include "base/uuid.h"

namespace sync_tab_context {

using ContainerId = base::StrongAlias<class ContainerIdTag, base::Uuid>;

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_CONTAINER_ID_H_
