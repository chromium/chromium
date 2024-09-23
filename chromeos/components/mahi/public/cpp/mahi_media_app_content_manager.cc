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

// static
ScopedMahiMediaAppContentManagerSetter*
    ScopedMahiMediaAppContentManagerSetter::instance_ = nullptr;

ScopedMahiMediaAppContentManagerSetter::ScopedMahiMediaAppContentManagerSetter(
    MahiMediaAppContentManager* proxy) {
  // Only allow one scoped instance at a time.
  if (instance_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  instance_ = this;

  // Save the real manager instance and replace it with the fake one.
  real_content_manager_instance_ = g_instance;
  g_instance = proxy;
}

ScopedMahiMediaAppContentManagerSetter::
    ~ScopedMahiMediaAppContentManagerSetter() {
  if (instance_ != this) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  instance_ = nullptr;

  g_instance = real_content_manager_instance_;
  real_content_manager_instance_ = nullptr;
}

}  // namespace chromeos
