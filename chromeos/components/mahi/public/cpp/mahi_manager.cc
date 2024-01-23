// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

#include "base/check_op.h"
#include "base/notreached.h"

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

// static
ScopedMahiManagerSetter* ScopedMahiManagerSetter::instance_ = nullptr;

ScopedMahiManagerSetter::ScopedMahiManagerSetter(MahiManager* manager) {
  // Only allow one scoped instance at a time.
  if (instance_) {
    NOTREACHED();
    return;
  }
  instance_ = this;

  // Save the real manager instance and replace it with the fake one.
  real_manager_instance_ = g_instance;
  g_instance = manager;
}

ScopedMahiManagerSetter::~ScopedMahiManagerSetter() {
  if (instance_ != this) {
    NOTREACHED();
    return;
  }

  instance_ = nullptr;

  g_instance = real_manager_instance_;
  real_manager_instance_ = nullptr;
}

}  // namespace chromeos
