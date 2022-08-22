// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/glanceables/chrome_glanceables_delegate.h"

#include "ash/glanceables/glanceables_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "chrome/common/chrome_switches.h"

namespace {
ChromeGlanceablesDelegate* g_instance = nullptr;
}  // namespace

ChromeGlanceablesDelegate::ChromeGlanceablesDelegate(
    ash::GlanceablesController* controller)
    : controller_(controller) {
  DCHECK(controller_);

  DCHECK(!g_instance);
  g_instance = this;
}

ChromeGlanceablesDelegate::~ChromeGlanceablesDelegate() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ChromeGlanceablesDelegate* ChromeGlanceablesDelegate::Get() {
  return g_instance;
}

void ChromeGlanceablesDelegate::OnPrimaryUserSessionStarted() {
  if (ShouldShowOnLogin())
    controller_->ShowOnLogin();
}

void ChromeGlanceablesDelegate::RestoreSession() {
  // TODO(crbug.com/1353119): Use the FullRestoreService to trigger session
  // restore.
  NOTIMPLEMENTED();
}

bool ChromeGlanceablesDelegate::ShouldShowOnLogin() const {
  // Skip glanceables when --no-first-run is passed. This prevents glanceables
  // from interfering with existing browser tests (they pass this switch) and is
  // also helpful when bisecting.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kNoFirstRun))
    return false;

  // TODO(crbug.com/1353119): Use logic from FullRestoreService to decide
  // whether or not to show.
  return true;
}
