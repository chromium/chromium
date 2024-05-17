// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_media_app_content_manager.h"

namespace chromeos {

namespace {
MahiMediaAppContentManager* g_instance = nullptr;
}  // namespace

// static
MahiMediaAppContentManager* MahiMediaAppContentManager::Get() {
  return g_instance;
}

MahiMediaAppContentManager::MahiMediaAppContentManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

MahiMediaAppContentManager::~MahiMediaAppContentManager() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace chromeos
