// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/experiences/arc/arc_platform_support.h"

#include "base/check_op.h"

namespace arc {

namespace {
ArcPlatformSupport* g_arc_platform_support = nullptr;
}  // namespace

ArcPlatformSupport::ArcPlatformSupport() {
  CHECK(!g_arc_platform_support);
  g_arc_platform_support = this;
}

ArcPlatformSupport::~ArcPlatformSupport() {
  CHECK_EQ(g_arc_platform_support, this);
  g_arc_platform_support = nullptr;
}

// static
ArcPlatformSupport* ArcPlatformSupport::Get() {
  return g_arc_platform_support;
}

}  // namespace arc
