// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/graduation/graduation_manager.h"

namespace ash::graduation {
namespace {
GraduationManager* g_instance = nullptr;
}

GraduationManager::GraduationManager() {
  CHECK_EQ(g_instance, nullptr);
  g_instance = this;

  // SessionManager may be unset in unit tests.
  auto* session_manager = session_manager::SessionManager::Get();
  if (session_manager) {
    session_manager_observation_.Observe(session_manager);
  }
}

GraduationManager::~GraduationManager() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
GraduationManager* GraduationManager::Get() {
  return g_instance;
}

void GraduationManager::OnUserSessionStarted(bool is_primary) {
  // TOOD(b/357882466): Implement adding initial app enablement state.
}
}  // namespace ash::graduation
