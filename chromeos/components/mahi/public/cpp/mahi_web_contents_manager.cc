// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/mahi/public/cpp/mahi_web_contents_manager.h"

#include "base/check_is_test.h"

namespace chromeos {

namespace {
MahiWebContentsManager* g_instance = nullptr;
MahiWebContentsManager* g_instance_for_testing = nullptr;

}  // namespace

// static
// TODO(b:356035887): this may return different ptrs to callers depending on
// timing in test. Minimize the use of this global getter, and get rid of
// overriding global instance while there exists a live one.
MahiWebContentsManager* MahiWebContentsManager::Get() {
  if (g_instance_for_testing) {
    return g_instance_for_testing;
  }
  return g_instance;
}

MahiWebContentsManager::MahiWebContentsManager() {
  // Multiple instances of MahiWebContentsManager are allowed only in tests.
  if (g_instance) {
    CHECK_IS_TEST();
  } else {
    g_instance = this;
  }
}

MahiWebContentsManager::~MahiWebContentsManager() {
  // Multiple instances of MahiWebContentsManager are allowed only in tests.
  if (g_instance != this) {
    CHECK_IS_TEST();
  } else {
    g_instance = nullptr;
  }
}

// static
ScopedMahiWebContentsManagerOverride*
    ScopedMahiWebContentsManagerOverride::instance_ = nullptr;
ScopedMahiWebContentsManagerOverride::ScopedMahiWebContentsManagerOverride(
    MahiWebContentsManager* delegate) {
  // Only allow one scoped instance at a time.
  if (instance_) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  instance_ = this;
  g_instance_for_testing = delegate;
}

ScopedMahiWebContentsManagerOverride::~ScopedMahiWebContentsManagerOverride() {
  if (instance_ != this) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  instance_ = nullptr;
  g_instance_for_testing = nullptr;
}

}  // namespace chromeos
