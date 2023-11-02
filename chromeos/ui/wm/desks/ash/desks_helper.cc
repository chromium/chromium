// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ui/wm/desks/desks_helper.h"

#include "base/check_op.h"
#include "ui/aura/window.h"

namespace chromeos {

namespace {
DesksHelper* g_instance = nullptr;
}  // namespace

// static
DesksHelper* DesksHelper::Get(aura::Window* window) {
  return g_instance;
}

DesksHelper::DesksHelper() {
  DCHECK(!g_instance);
  g_instance = this;
}

DesksHelper::~DesksHelper() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace chromeos
