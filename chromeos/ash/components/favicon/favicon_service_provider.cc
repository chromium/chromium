// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/favicon/favicon_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
FaviconServiceProvider* g_instance = nullptr;
}  // namespace

FaviconServiceProvider::FaviconServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

FaviconServiceProvider::~FaviconServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
FaviconServiceProvider& FaviconServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
