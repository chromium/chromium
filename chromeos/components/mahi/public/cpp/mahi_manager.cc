// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

#include "base/check_op.h"

namespace chromeos {

namespace {

MahiManager* g_instance = nullptr;

}  // namespace

// static
MahiManager* MahiManager::Get() {
  return g_instance;
}

MahiManager::MahiManager() {
  DCHECK(!g_instance);
  g_instance = this;
}

MahiManager::~MahiManager() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace chromeos
