// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_H_
#define COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

namespace sync_tab_context {

class TabContextSyncService : public KeyedService {
 public:
  TabContextSyncService();
  ~TabContextSyncService() override;

  TabContextSyncService(const TabContextSyncService&) = delete;
  TabContextSyncService& operator=(const TabContextSyncService&) = delete;
};

}  // namespace sync_tab_context

#endif  // COMPONENTS_SYNC_TAB_CONTEXT_TAB_CONTEXT_SYNC_SERVICE_H_
