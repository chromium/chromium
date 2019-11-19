// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/arc_service_manager.h"

#include "base/logging.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/arc/session/arc_session.h"
#include "components/arc/session/arc_session_runner.h"

namespace arc {
namespace {

// Weak pointer.  This class is owned by arc::ArcServiceLauncher.
ArcServiceManager* g_arc_service_manager = nullptr;

}  // namespace

ArcServiceManager::ArcServiceManager()
    : arc_bridge_service_(std::make_unique<ArcBridgeService>()) {
  DCHECK(!g_arc_service_manager);
  g_arc_service_manager = this;
}

ArcServiceManager::~ArcServiceManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(g_arc_service_manager, this);
  g_arc_service_manager = nullptr;
}

// static
ArcServiceManager* ArcServiceManager::Get() {
  if (!g_arc_service_manager)
    return nullptr;
  DCHECK_CALLED_ON_VALID_THREAD(g_arc_service_manager->thread_checker_);
  return g_arc_service_manager;
}

ArcBridgeService* ArcServiceManager::arc_bridge_service() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return arc_bridge_service_.get();
}

}  // namespace arc
